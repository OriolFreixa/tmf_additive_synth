#include "tmf_additive_synth/tmf_additive_synth.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{
    class StaggeredSpectrumProbe : public tmf::AdditiveSynthHarmonicCollector
    {
    public:
        static std::string getDisplayNameStatic() { return "Staggered spectrum"; }
        void collectHarmonics (juce::AudioBuffer<float>& buffer, size_t count) override
        {
            ++rebuilds;
            if (count <= 2)
                return;
            buffer.setSample (0, 2, level.getTargetValue());
            buffer.setSample (1, 2, level.getTargetValue() * 0.3f);
        }
        int rebuilds = 0;
    };

    struct StaggeredVoices
    {
        explicit StaggeredVoices (double sampleRate = 48000)
            : collectorOwner (std::make_shared<tmf::HarmonicCollectorManager<StaggeredSpectrumProbe>>()),
              owner ({ collectorOwner })
        {
            owner.applyParameterSnapshot (std::array<float, 3> { 0.5f, 0.0f, 0.0f });
            for (int index = 0; index < 8; ++index)
            {
                auto voice = std::dynamic_pointer_cast<tmf::VoiceInterceptorAdditiveSynth> (owner.getOrCreateVoiceInterceptor (index));
                voice->prepareToPlay (sampleRate, 128, 2);
                voice->startNote (60 + index, 1.0f, 8192);
                voices.push_back (voice);
                collectors.push_back (std::dynamic_pointer_cast<StaggeredSpectrumProbe> (collectorOwner->getOrCreateHarmonicCollector (static_cast<size_t> (index))));
                bindings.push_back (voice->bindModulationTarget (collectorOwner->getId() + "_LEVEL"));
            }
        }
        void automate (int sample)
        {
            for (auto& binding : bindings)
                binding.update ((sample / 32) % 2 == 0 ? -0.25f : 0.25f);
        }
        void render (juce::AudioBuffer<float>& output, int start, int count)
        {
            for (auto& voice : voices)
                voice->processBlock (output, start, count);
        }
        int totalRebuilds() const
        {
            int total = 0;
            for (const auto& collector : collectors)
                total += collector->rebuilds;
            return total;
        }
        std::shared_ptr<tmf::HarmonicCollectorManager<StaggeredSpectrumProbe>> collectorOwner;
        tmf::VoiceInterceptorAdditiveSynthManager owner;
        std::vector<std::shared_ptr<tmf::VoiceInterceptorAdditiveSynth>> voices;
        std::vector<std::shared_ptr<StaggeredSpectrumProbe>> collectors;
        std::vector<tmf::ModulationTargetBinding> bindings;
    };

    juce::AudioBuffer<float> renderStaggeredAutomation (int subdivision)
    {
        StaggeredVoices fixture;
        juce::AudioBuffer<float> output (2, 2400);
        output.clear();
        for (int sample = 0; sample < output.getNumSamples();)
        {
            if (sample % 32 == 0)
                fixture.automate (sample);
            const int count = std::min ({ subdivision, 32 - sample % 32, output.getNumSamples() - sample });
            fixture.render (output, sample, count);
            sample += count;
        }
        return output;
    }
}

TEST_CASE ("Periodic wavetable rebuilds spread across eight voices after immediate note starts",
    "[tmf_additive_synth][refresh-stagger]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    StaggeredVoices fixture (sampleRate);
    juce::AudioBuffer<float> output (2, 1);
    output.clear();
    fixture.render (output, 0, 1);
    CHECK (fixture.totalRebuilds() == 8);
    int windowRebuilds = 0;
    const int interval = static_cast<int> (std::round (sampleRate * 0.01));
    for (int sample = 1; sample < interval * 5; ++sample)
    {
        if (sample % 32 == 0)
            fixture.automate (sample);
        const int previous = fixture.totalRebuilds();
        output.clear();
        fixture.render (output, 0, 1);
        windowRebuilds += fixture.totalRebuilds() - previous;
        if (sample % 32 == 31)
        {
            CHECK (windowRebuilds <= 2);
            windowRebuilds = 0;
        }
    }
    for (const auto& collector : fixture.collectors)
        CHECK (collector->rebuilds >= 5);
}

TEST_CASE ("Staggered refresh timing is independent of render subdivision", "[tmf_additive_synth][refresh-stagger]")
{
    const auto subdivision = GENERATE (7, 17, 128, 1024);
    const auto reference = renderStaggeredAutomation (1);
    const auto actual = renderStaggeredAutomation (subdivision);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < actual.getNumSamples(); ++sample)
            CHECK (actual.getSample (channel, sample) == Catch::Approx (reference.getSample (channel, sample)).margin (1.0e-6f));
}

TEST_CASE ("Clean intervals retain stagger phases and pitch changes remain immediate", "[tmf_additive_synth][refresh-stagger]")
{
    StaggeredVoices fixture;
    juce::AudioBuffer<float> output (2, 1017);
    output.clear();
    fixture.render (output, 0, 1017);
    CHECK (fixture.totalRebuilds() == 8);
    fixture.automate (32);
    fixture.render (output, 0, 1);
    CHECK (fixture.collectors[0]->rebuilds == 2);
    CHECK (fixture.collectors[4]->rebuilds == 1);
    fixture.render (output, 0, 3);
    fixture.render (output, 0, 1); // Voice four's 60-sample slot: 960 + 60.
    CHECK (fixture.collectors[4]->rebuilds == 2);
    for (auto& voice : fixture.voices)
        voice->pitchWheelMoved (16383);
    const int beforePitch = fixture.totalRebuilds();
    fixture.render (output, 0, 1);
    CHECK (fixture.totalRebuilds() == beforePitch + 8);
    fixture.voices[1]->clearCurrentNote();
    fixture.voices[1]->startNote (60, 1.0f, 8192);
    const int beforeRestart = fixture.collectors[1]->rebuilds;
    fixture.voices[1]->processBlock (output, 0, 0);
    CHECK (fixture.collectors[1]->rebuilds == beforeRestart);
    fixture.voices[1]->processBlock (output, 0, 1);
    CHECK (fixture.collectors[1]->rebuilds == beforeRestart + 1);
    const int voiceSevenAfterPitch = fixture.collectors[7]->rebuilds;
    fixture.voices[7]->processBlock (output, 0, 357);
    CHECK (fixture.collectors[7]->rebuilds == voiceSevenAfterPitch);
    fixture.voices[7]->processBlock (output, 0, 1);
    CHECK (fixture.collectors[7]->rebuilds == voiceSevenAfterPitch + 1);
}

TEST_CASE ("A smoothing first table waits for its scheduled refresh", "[tmf_additive_synth][refresh-stagger]")
{
    StaggeredVoices fixture;
    fixture.automate (0);
    juce::AudioBuffer<float> output (2, 1);
    for (int sample = 0; sample < 480; ++sample)
    {
        output.clear();
        fixture.render (output, 0, 1);
    }
    CHECK (fixture.collectors[0]->rebuilds == 1);
    fixture.render (output, 0, 1);
    CHECK (fixture.collectors[0]->rebuilds == 2);
}
