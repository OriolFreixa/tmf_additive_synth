#include "tmf_additive_synth/tmf_additive_synth.h"
#include "../../tmf_intercept_synth/tests/harness/InterceptorOutputRegression.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    juce::AudioBuffer<float> collectHarmonics (tmf::AdditiveSynthHarmonicCollector& collector, int tableSize)
    {
        juce::AudioBuffer<float> buffer { 2, tableSize };
        buffer.clear();

        collector.prepareToPlay (44100.0, buffer.getNumChannels());
        collector.setParams ({ 1.0f, 0.0f, 0 });
        collector.collectHarmonics (buffer, static_cast<size_t> (tableSize));

        return buffer;
    }

    juce::AudioBuffer<float> renderAdditiveVoice (const std::shared_ptr<tmf::AdditiveSynthHarmonicCollector>& collector, int blockSize)
    {
        tmf::VoiceInterceptorAdditiveSynth voice;
        voice.addHarmonicCollector (collector);
        voice.prepareToPlay (juce::MidiMessage::getMidiNoteInHertz (69) * static_cast<double> (blockSize), blockSize, 2);
        voice.startNote (69, 1.0f, 8192);

        juce::AudioBuffer<float> buffer { 2, blockSize };
        buffer.clear();
        voice.processBlock (buffer, 0, blockSize);

        return buffer;
    }
}

TEST_CASE ("Sine harmonic collector output stays sample-stable", "[tmf_additive_synth][output-regression][collector]")
{
    tmf::HarmonicCollectorSine sine;
    const auto output = collectHarmonics (sine, 8);

    tmf::test::expectBufferMatches (output,
        {
            { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        });
}

TEST_CASE ("Octaves harmonic collector output stays sample-stable", "[tmf_additive_synth][output-regression][collector]")
{
    tmf::HarmonicCollectorOctaves octaves;
    octaves.setParamsValues ({ 1, 4 });
    const auto output = collectHarmonics (octaves, 20);

    tmf::test::expectBufferMatches (output,
        {
            { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
        });
}

TEST_CASE ("Shifted octaves harmonic collector output stays sample-stable", "[tmf_additive_synth][output-regression][collector]")
{
    tmf::HarmonicCollectorOctaves octaves { 2 };
    octaves.setParamsValues ({ 1, 3 });
    const auto output = collectHarmonics (octaves, 28);

    tmf::test::expectBufferMatches (output,
        {
            { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
        });
}

TEST_CASE ("Octave harmonics collector output stays sample-stable", "[tmf_additive_synth][output-regression][collector]")
{
    tmf::HarmonicCollectorOctaveHarmonicsParams params;
    params.levels.fill (0.0f);
    params.pans.fill (0.0f);
    params.levels[0] = 1.0f;
    params.pans[0] = -100.0f;
    params.levels[1] = 0.5f;
    params.pans[1] = 100.0f;
    params.levels[2] = 0.25f;

    tmf::HarmonicCollectorOctaveHarmonics harmonics;
    harmonics.setHarmonicParams (params);
    const auto output = collectHarmonics (harmonics, 20);

    tmf::test::expectBufferMatches (output,
        {
            { 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        });
}

TEST_CASE ("Noise band harmonic collector output stays sample-stable", "[tmf_additive_synth][output-regression][collector]")
{
    tmf::HarmonicCollectorUnevenBands noise { 0, 3, 7 };
    const auto output = collectHarmonics (noise, 20);

    tmf::test::expectBufferMatches (output,
        {
            { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        });
}

TEST_CASE ("EnFifther harmonic collector output stays sample-stable", "[tmf_additive_synth][output-regression][collector]")
{
    juce::AudioBuffer<float> output { 2, 20 };
    output.clear();
    output.setSample (0, 2, 1.0f);
    output.setSample (1, 2, 1.0f);
    output.setSample (0, 4, 0.5f);
    output.setSample (1, 4, 0.5f);

    tmf::HarmonicCollectorEnFifther enFifther;
    enFifther.prepareToPlay (44100.0, output.getNumChannels());
    enFifther.setParams ({ 1.0f, 0.0f, 0 });
    enFifther.collectHarmonics (output, static_cast<size_t> (output.getNumSamples()));

    tmf::test::expectBufferMatches (output,
        {
            { 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        });
}

TEST_CASE ("Additive synth converts sine harmonics into a stable waveform", "[tmf_additive_synth][output-regression][voice]")
{
    auto sine = std::make_shared<tmf::HarmonicCollectorSine>();
    sine->setParams ({ 1.0f, 0.0f, 0 });

    const auto output = renderAdditiveVoice (sine, 8);

    tmf::test::expectBufferMatches (output,
        {
            { 2.0f, 1.414213538f, 0.0f, -1.414213538f, -2.0f, -1.4142133f, 0.0f, 1.4142133f },
            { 2.0f, 1.414213538f, 0.0f, -1.414213538f, -2.0f, -1.4142133f, 0.0f, 1.4142133f },
        },
        0.00001);
}

TEST_CASE ("Additive synth converts octave harmonics into a stable summed waveform", "[tmf_additive_synth][output-regression][voice]")
{
    auto octaves = std::make_shared<tmf::HarmonicCollectorOctaves>();
    octaves->setParams ({ 1.0f, 0.0f, 0 });
    octaves->setParamsValues ({ 1, 3 });

    const auto output = renderAdditiveVoice (octaves, 8);

    tmf::test::expectBufferMatches (output,
        {
            { 3.5f, 0.914213538f, -0.5f, -1.914213538f, -0.5f, -1.9142133f, -0.5f, 0.9142133f },
            { 3.5f, 0.914213538f, -0.5f, -1.914213538f, -0.5f, -1.9142133f, -0.5f, 0.9142133f },
        },
        0.00001);
}
