#include "tmf_additive_synth/tmf_additive_synth.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{
    class SpectrumRefreshProbe final : public tmf::AdditiveSynthHarmonicCollector
    {
    public:
        static std::string getDisplayNameStatic() { return "Spectrum Refresh Probe"; }
        void collectHarmonics (juce::AudioBuffer<float>& buffer, size_t count) override
        {
            ++refreshCount;
            lastRenderedLevel = level.getCurrentValue();
            if (count > 2)
                buffer.setSample (0, 2, lastRenderedLevel);
        }
        int refreshCount = 0;
        float lastRenderedLevel = 0.0f;
    };

    struct AdditiveRefreshFixture
    {
        AdditiveRefreshFixture()
            : collector (std::make_shared<SpectrumRefreshProbe>())
        {
            collector->setId ("REFRESH");
            collector->setParams ({ 0.5f, 0.0f, 1 });
            voice.addHarmonicCollector (collector);
            voice.prepareToPlay (48000.0, 32, 2);
            voice.startNote (60, 1.0f, 8192);
            renderSlices (1);
        }
        void renderSlices (int count)
        {
            for (int slice = 0; slice < count; ++slice)
            {
                buffer.clear();
                voice.processBlock (buffer, 0, 32);
            }
        }
        std::shared_ptr<SpectrumRefreshProbe> collector;
        tmf::VoiceInterceptorAdditiveSynth voice;
        juce::AudioBuffer<float> buffer { 2, 32 };
    };

    struct StaticAdditiveRouteFixture
    {
        explicit StaticAdditiveRouteFixture (float depth)
            : synth ({ additive, macro }, 1),
              apvts (processor, nullptr, "parameters", synth.createParameterLayout())
        {
            synth.setupAPVTS (apvts);
            setParameter (collectorManager->getId() + "_ORDER", 1.0f);
            setParameter ("MACRO_1", 1.0f);
            REQUIRE (synth.addModulationRoute (apvts, "macro_1", collectorManager->getId() + "_LEVEL")
                     == tmf::ModulationRouteAddResult::added);
            setParameter ("MODMATRIX_AMOUNT_0", depth);
            synth.prepareToPlay (48000.0, 1024, 2);
        }

        void setParameter (const juce::String& id, float value)
        {
            auto* parameter = apvts.getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }

        std::shared_ptr<tmf::HarmonicCollectorManager<SpectrumRefreshProbe>> collectorManager =
            std::make_shared<tmf::HarmonicCollectorManager<SpectrumRefreshProbe>>();
        std::shared_ptr<tmf::VoiceInterceptorAdditiveSynthManager> additive =
            std::make_shared<tmf::VoiceInterceptorAdditiveSynthManager> (
                std::vector<std::shared_ptr<tmf::HarmonicCollectorManagerInterface>> { collectorManager });
        std::shared_ptr<tmf::MacroModulationSourceManager> macro =
            std::make_shared<tmf::MacroModulationSourceManager> (1);
        tmf::InterceptSynth synth;
        juce::AudioProcessorGraph processor;
        juce::AudioProcessorValueTreeState apvts;
    };

}

TEST_CASE ("Unchanged additive offsets leave a settled wavetable intact", "[tmf_additive_synth][modulation-refresh]")
{
    AdditiveRefreshFixture fixture;
    const auto target = GENERATE ("REFRESH_LEVEL", "REFRESH_PAN", "REFRESH_ORDER");
    for (int tick = 0; tick < 40; ++tick)
    {
        fixture.voice.updateModTargetValue (target, 0.0f);
        fixture.renderSlices (1);
    }
    CHECK (fixture.collector->refreshCount == 1);
    fixture.voice.updateModTargetValue ("UNKNOWN_LEVEL", 0.5f);
    fixture.renderSlices (1);
    CHECK (fixture.collector->refreshCount == 1);
}

TEST_CASE ("Additive smoothing renders its final spectrum then stops rebuilding", "[tmf_additive_synth][modulation-refresh]")
{
    AdditiveRefreshFixture fixture;
    fixture.voice.updateModTargetValue ("REFRESH_LEVEL", 0.25f);
    fixture.renderSlices (20);
    CHECK (fixture.collector->lastRenderedLevel == Catch::Approx (0.75f));
    const auto settledRefreshCount = fixture.collector->refreshCount;
    CHECK (settledRefreshCount > 1);
    for (int tick = 0; tick < 40; ++tick)
    {
        fixture.voice.updateModTargetValue ("REFRESH_LEVEL", 0.25f);
        fixture.renderSlices (1);
    }
    CHECK (fixture.collector->refreshCount == settledRefreshCount);
    fixture.voice.updateModTargetValue ("REFRESH_LEVEL", 0.0f);
    fixture.renderSlices (20);
    CHECK (fixture.collector->lastRenderedLevel == Catch::Approx (0.5f));
    CHECK (fixture.collector->refreshCount > settledRefreshCount);
}

TEST_CASE ("Clamped additive offsets retain their raw value without redundant rebuilds", "[tmf_additive_synth][modulation-refresh]")
{
    AdditiveRefreshFixture fixture;
    fixture.voice.updateModTargetValue ("REFRESH_LEVEL", 0.75f);
    fixture.renderSlices (20);
    const auto settledRefreshCount = fixture.collector->refreshCount;
    fixture.voice.updateModTargetValue ("REFRESH_LEVEL", 0.6f);
    fixture.renderSlices (20);
    CHECK (fixture.collector->refreshCount == settledRefreshCount);
    fixture.collector->setParams ({ 0.1f, 0.0f, 1 });
    fixture.voice.markWavetableAsNeedsRefresh();
    fixture.renderSlices (20);
    CHECK (fixture.collector->lastRenderedLevel == Catch::Approx (0.7f));
}

TEST_CASE ("Quantized additive order offsets rebuild only when effective order changes", "[tmf_additive_synth][modulation-refresh]")
{
    AdditiveRefreshFixture fixture;
    fixture.voice.updateModTargetValue ("REFRESH_ORDER", 0.0001f);
    fixture.renderSlices (1);
    CHECK (fixture.collector->refreshCount == 1);
    fixture.voice.updateModTargetValue ("REFRESH_ORDER", 0.01f);
    fixture.renderSlices (1);
    CHECK (fixture.collector->refreshCount == 2);
    CHECK (fixture.collector->getOrder() > 1);
    fixture.voice.updateModTargetValue ("REFRESH_ORDER", 0.01f);
    fixture.renderSlices (1);
    CHECK (fixture.collector->refreshCount == 2);
}

TEST_CASE ("Static macro and zero-depth additive routes settle at the internal cadence", "[tmf_additive_synth][modulation-refresh]")
{
    const auto depth = GENERATE (0.0f, 0.25f);
    StaticAdditiveRouteFixture fixture { depth };
    juce::AudioBuffer<float> buffer { 2, 1024 };
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    fixture.synth.renderNextBlock (buffer, midi, 0, 1024);
    const auto collector = std::dynamic_pointer_cast<SpectrumRefreshProbe> (
        fixture.collectorManager->getOrCreateHarmonicCollector (0));
    REQUIRE (collector != nullptr);
    CHECK (collector->lastRenderedLevel == Catch::Approx (0.5f + depth));
    const auto settledRefreshCount = collector->refreshCount;
    midi.clear();
    buffer.clear();
    fixture.synth.renderNextBlock (buffer, midi, 0, 1024);
    CHECK (collector->refreshCount == settledRefreshCount);
}
