/*
  ==============================================================================

    AdditiveWavetableMath.h

    Shared constants and spectrum helpers for the packed real-FFT wavetable
    used by the additive voice and by editors that preview the same wave.

  ==============================================================================
*/

#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

static constexpr int waveTableOrder = 11;
static constexpr int waveTableSize = 1 << waveTableOrder;
static constexpr double radiantsPerSample = juce::MathConstants<double>::twoPi / (double) (waveTableSize);

namespace tmf::additiveWavetable
{
    /// Largest mid-channel magnitude one harmonic may reach before it saturates.
    /// Example: two full-level collectors stacked on harmonic 3 still leave that harmonic at 1.
    inline constexpr float maxHarmonicMagnitude = 1.0f;

    /// Returns the per-entry 1/n weight applied to a packed spectrum before the inverse FFT.
    /// The real and imaginary entries of harmonic n share the weight, so collectors can write
    /// either phase with the same "level 1 equals a 1/n spectrum" meaning.
    /// Example: `createHarmonicVolumeMultipliers()[6]` is 1/3 and so is entry 7.
    inline std::vector<float> createHarmonicVolumeMultipliers()
    {
        std::vector<float> multipliers (static_cast<size_t> (waveTableSize), 1.0f);
        for (int index = 2; index + 1 < waveTableSize; index += 2)
        {
            const auto weight = 1.0f / static_cast<float> (index / 2);
            multipliers[static_cast<size_t> (index)] = weight;
            multipliers[static_cast<size_t> (index) + 1u] = weight;
        }

        return multipliers;
    }

    /// Scales both channels of every overlapping harmonic so its mid-channel magnitude stays at or below
    /// `maxHarmonicMagnitude`. Panning is preserved because both channels share the scale.
    /// A single collector never exceeds the limit, so existing patches are unchanged.
    /// Example: left 3 and right 1 on harmonic 2 (mid 2) become left 1.5 and right 0.5.
    inline void saturateStereoHarmonics (juce::AudioBuffer<float>& table)
    {
        jassert (table.getNumChannels() >= 2);
        if (table.getNumChannels() < 2)
            return;

        auto* left = table.getWritePointer (0);
        auto* right = table.getWritePointer (1);
        const auto entryCount = static_cast<size_t> (table.getNumSamples());
        for (size_t index = 2; index + 1 < entryCount; index += 2)
        {
            const auto midReal = 0.5f * (left[index] + right[index]);
            const auto midImaginary = 0.5f * (left[index + 1] + right[index + 1]);
            const auto magnitude = std::hypot (midReal, midImaginary);
            if (magnitude <= maxHarmonicMagnitude)
                continue;

            const auto scale = maxHarmonicMagnitude / magnitude;
            left[index] *= scale;
            left[index + 1] *= scale;
            right[index] *= scale;
            right[index + 1] *= scale;
        }
    }
}
