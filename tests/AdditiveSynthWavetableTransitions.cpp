#include "tmf_additive_synth/tmf_additive_synth.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
    struct HarmonicTransitionFixture
    {
        explicit HarmonicTransitionFixture (double sampleRate)
            : collector (std::make_shared<tmf::HarmonicCollectorOctaveHarmonicsManager> (1)),
              manager ({ collector }),
              snapshot (3 + 2 * tmf::maxBoundValue, 0.0f)
        {
            snapshot[0] = 1.0f;
            snapshot[1] = 0.0f;
            snapshot[3] = 1.0f;
            manager.applyParameterSnapshot (snapshot);
            voice = std::dynamic_pointer_cast<tmf::VoiceInterceptorAdditiveSynth> (manager.getOrCreateVoiceInterceptor (0));
            voice->prepareToPlay (sampleRate, 128, 2);
            voice->startNote (60, 1.0f, 8192);
        }

        juce::AudioBuffer<float> render (int count)
        {
            juce::AudioBuffer<float> result (2, count);
            result.clear();
            voice->processBlock (result, 0, count);
            return result;
        }

        void setHarmonic (float level, float pan)
        {
            snapshot[3] = level;
            snapshot[4] = pan;
            manager.applyParameterSnapshot (snapshot);
        }

        void modulateHarmonic()
        {
            const auto descriptions = collector->getParameterDescriptions();
            voice->updateModTargetValue (descriptions[3].id, -0.75f);
            voice->updateModTargetValue (descriptions[4].id, 0.5f);
        }

        std::shared_ptr<tmf::HarmonicCollectorOctaveHarmonicsManager> collector;
        tmf::VoiceInterceptorAdditiveSynthManager manager;
        std::vector<float> snapshot;
        std::shared_ptr<tmf::VoiceInterceptorAdditiveSynth> voice;
    };

    void checkBlendedRender (HarmonicTransitionFixture& edited, HarmonicTransitionFixture& reference, int totalSamples, int blockSize, int fadeLength, float sourceLeft, float sourceRight, float targetLeft, float targetRight, bool repeatSnapshot = false)
    {
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            if (repeatSnapshot)
                edited.manager.applyParameterSnapshot (edited.snapshot);
            const auto count = std::min (blockSize, totalSamples - offset);
            const auto actual = edited.render (count);
            const auto baseline = reference.render (count);
            for (int sample = 0; sample < count; ++sample)
            {
                const auto alpha = std::min (1.0f, static_cast<float> (offset + sample) / static_cast<float> (fadeLength));
                const auto left = sourceLeft + alpha * (targetLeft - sourceLeft);
                const auto right = sourceRight + alpha * (targetRight - sourceRight);
                INFO ("sample " << offset + sample << ", block size " << blockSize);
                CHECK (actual.getSample (0, sample) == Catch::Approx (baseline.getSample (0, sample) * left).margin (0.000002f));
                CHECK (actual.getSample (1, sample) == Catch::Approx (baseline.getSample (1, sample) * right).margin (0.000002f));
            }
        }
    }
}

TEST_CASE ("Harmonic edits crossfade over ten milliseconds independently of render blocks",
    "[tmf_additive_synth][voice][transition]")
{
    const auto useModulation = GENERATE (false, true);
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
        for (const auto blockSize : { 1, 64, 1024 })
        {
            INFO ("sample rate " << sampleRate);
            HarmonicTransitionFixture edited (sampleRate);
            HarmonicTransitionFixture reference (sampleRate);
            edited.render (37);
            reference.render (37);
            const auto fadeLength = static_cast<int> (std::round (sampleRate * 0.01));
            if (useModulation)
                edited.modulateHarmonic();
            else
                edited.setHarmonic (0.25f, 100.0f);
            checkBlendedRender (edited, reference, fadeLength + 16, blockSize, fadeLength, 1.0f, 1.0f, 0.0f, 0.5f);
        }
}

TEST_CASE ("Harmonic fades coalesce pending edits until the next table interval",
    "[tmf_additive_synth][voice][transition]")
{
    HarmonicTransitionFixture edited (48000.0);
    HarmonicTransitionFixture reference (48000.0);
    edited.render (37);
    reference.render (37);
    edited.setHarmonic (0.25f, 100.0f);
    checkBlendedRender (edited, reference, 240, 64, 480, 1.0f, 1.0f, 0.0f, 0.5f, true);
    edited.setHarmonic (0.9f, 0.0f);
    edited.setHarmonic (0.5f, -100.0f);
    edited.voice->pitchWheelMoved (8192);
    checkBlendedRender (edited, reference, 240, 64, 240, 0.5f, 0.75f, 0.0f, 0.5f, true);
    checkBlendedRender (edited, reference, 500, 64, 480, 0.0f, 0.5f, 1.0f, 0.0f, true);
}

TEST_CASE ("Fresh and reused additive voices install the first table immediately",
    "[tmf_additive_synth][voice][transition]")
{
    HarmonicTransitionFixture edited (48000.0);
    HarmonicTransitionFixture reference (48000.0);
    checkBlendedRender (edited, reference, 37, 16, 480, 1.0f, 1.0f, 1.0f, 1.0f);
    edited.setHarmonic (0.25f, 100.0f);
    checkBlendedRender (edited, reference, 120, 32, 480, 1.0f, 1.0f, 0.0f, 0.5f);
    SECTION ("Clear and reuse")
    {
        edited.voice->clearCurrentNote();
        edited.setHarmonic (0.5f, -100.0f);
    }
    SECTION ("Prepare during a fade")
    {
        edited.voice->prepareToPlay (48000.0, 128, 2);
        edited.voice->clearCurrentNote();
        edited.setHarmonic (0.5f, -100.0f);
    }
    edited.voice->startNote (60, 1.0f, 8192);
    checkBlendedRender (edited, reference, 64, 16, 480, 1.0f, 0.0f, 1.0f, 0.0f);
}

TEST_CASE ("Adding a partial blends complete waveforms at the same oscillator phase",
    "[tmf_additive_synth][voice][transition]")
{
    HarmonicTransitionFixture edited (48000.0);
    HarmonicTransitionFixture fundamental (48000.0);
    HarmonicTransitionFixture secondPartial (48000.0);
    secondPartial.snapshot[3] = 0.0f;
    secondPartial.snapshot[5] = 1.0f;
    secondPartial.snapshot[6] = 100.0f;
    secondPartial.manager.applyParameterSnapshot (secondPartial.snapshot);
    edited.render (37);
    fundamental.render (37);
    secondPartial.render (37);
    edited.snapshot[5] = 0.75f;
    edited.snapshot[6] = 100.0f;
    edited.manager.applyParameterSnapshot (edited.snapshot);
    const auto actual = edited.render (500);
    const auto baseline = fundamental.render (500);
    const auto partial = secondPartial.render (500);
    for (int sample = 0; sample < 500; ++sample)
    {
        const auto alpha = std::min (1.0f, static_cast<float> (sample) / 480.0f);
        CHECK (actual.getSample (0, sample) == Catch::Approx (baseline.getSample (0, sample)).margin (0.000002f));
        CHECK (actual.getSample (1, sample)
               == Catch::Approx (baseline.getSample (1, sample) + alpha * 0.75f * partial.getSample (1, sample)).margin (0.000002f));
    }
}

TEST_CASE ("Pitch cutoff changes discard the old table immediately",
    "[tmf_additive_synth][voice][transition]")
{
    HarmonicTransitionFixture edited (48000.0);
    HarmonicTransitionFixture reference (48000.0);
    for (auto* fixture : { &edited, &reference })
    {
        fixture->snapshot[13] = 1.0f; // Harmonic 32 crosses Nyquist on this upward bend.
        fixture->manager.applyParameterSnapshot (fixture->snapshot);
        fixture->voice->startNote (77, 1.0f, 8192);
        fixture->render (37);
    }
    edited.voice->pitchWheelMoved (16383);
    reference.voice->startNote (77, 1.0f, 16383);
    checkBlendedRender (edited, reference, 500, 64, 480, 1.0f, 1.0f, 1.0f, 1.0f);
    edited.setHarmonic (0.25f, 100.0f);
    checkBlendedRender (edited, reference, 500, 64, 480, 1.0f, 1.0f, 0.0f, 0.5f);
}
