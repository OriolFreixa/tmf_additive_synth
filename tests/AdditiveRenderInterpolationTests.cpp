#include "tmf_additive_synth/tmf_additive_synth.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{
    juce::AudioBuffer<float> referenceSpectrum (const tmf::HarmonicCollectorOctaveHarmonicsParams& parameters)
    {
        tmf::HarmonicCollectorOctaveHarmonics collector;
        collector.setParams ({ 1.0f, 0.0f, 0 });
        collector.setHarmonicParams (parameters);
        collector.prepareToPlay (48000.0, 2);
        collector.setCurrentNote (69);
        juce::AudioBuffer<float> table (2, waveTableSize);
        table.clear();
        collector.collectHarmonics (table, waveTableSize);
        FourierTransform fft (waveTableOrder);
        for (int channel = 0; channel < 2; ++channel)
        {
            auto* spectrum = table.getWritePointer (channel);
            for (int bin = 2; bin < waveTableSize; bin += 2)
                spectrum[bin] /= static_cast<float> (bin / 2);
            fft.transformRealInverse (spectrum);
        }
        return table;
    }

    float referenceSample (const juce::AudioBuffer<float>& table, float phase, int channel)
    {
        const double position = phase / radiantsPerSample;
        const int first = static_cast<int> (position);
        const double fraction = position - first;
        const int second = first + 1 == waveTableSize ? 0 : first + 1;
        return static_cast<float> (table.getSample (channel, first) * (1.0 - fraction)
                                   + table.getSample (channel, second) * fraction);
    }
}

TEST_CASE ("Shared interpolation matches scalar reference samples through wraps and fade completion",
    "[tmf_additive_synth][interpolation]")
{
    const auto channels = GENERATE (1, 2);
    const auto sampleRate = GENERATE (32000.0, 44100.0, 96000.0);
    auto collector = std::make_shared<tmf::HarmonicCollectorOctaveHarmonics>();
    collector->setId ("REFERENCE");
    collector->setParams ({ 1.0f, 0.0f, 0 });
    tmf::HarmonicCollectorOctaveHarmonicsParams parameters;
    parameters.levels.fill (0.0f);
    parameters.levels[0] = 0.7f;
    parameters.levels[1] = 0.37f;
    parameters.pans[1] = -29.0f;
    collector->setHarmonicParams (parameters);
    const auto before = referenceSpectrum (parameters);
    tmf::VoiceInterceptorAdditiveSynth voice;
    voice.addHarmonicCollector (collector);
    voice.prepareToPlay (sampleRate, 128, channels);
    voice.startNote (69, 1.0f, 8192);
    float phase = 0.0f;
    const float increment = juce::MathConstants<float>::twoPi * 440.0f / static_cast<float> (sampleRate);
    juce::AudioBuffer<float> initial (channels, 137);
    initial.clear();
    voice.processBlock (initial, 0, 137);
    for (int sample = 0; sample < 137; ++sample)
    {
        for (int channel = 0; channel < channels; ++channel)
            CHECK (initial.getSample (channel, sample) == Catch::Approx (referenceSample (before, phase, channel)).margin (1.0e-6f));
        phase += increment;
        if (phase >= juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
    }
    parameters.levels[0] = 0.25f;
    parameters.pans[0] = 73.0f;
    collector->setHarmonicParams (parameters);
    voice.markWavetableAsNeedsRefresh();
    const auto after = referenceSpectrum (parameters);
    const int fadeLength = static_cast<int> (std::round (sampleRate * 0.01));
    int elapsed = 0;
    for (const auto count : { 1, 17, 137, 2048 })
    {
        juce::AudioBuffer<float> output (channels, count + 10);
        for (int channel = 0; channel < channels; ++channel)
            std::fill_n (output.getWritePointer (channel), output.getNumSamples(), 0.125f);
        voice.processBlock (output, 5, count);
        for (int sample = 0; sample < count; ++sample, ++elapsed)
        {
            const float blend = std::min (1.0f, static_cast<float> (elapsed) / static_cast<float> (fadeLength));
            for (int channel = 0; channel < channels; ++channel)
            {
                const float target = referenceSample (after, phase, channel);
                const float source = referenceSample (before, phase, channel);
                const float expected = elapsed < fadeLength ? source + blend * (target - source) : target;
                CHECK (output.getSample (channel, sample + 5) == Catch::Approx (expected + 0.125f).margin (1.0e-6f));
            }
            phase += increment;
            if (phase >= juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }
        for (int channel = 0; channel < channels; ++channel)
        {
            CHECK (output.getSample (channel, 0) == 0.125f);
            CHECK (output.getSample (channel, count + 5) == 0.125f);
        }
    }
}
