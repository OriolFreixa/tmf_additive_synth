#pragma once

#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <span>
#include <vector>

namespace tmf::harmonicStereo
{
    /// Alternates only audible spectrum entries, ignoring FFT residue and absent parity harmonics.
    /// Example: odd-only harmonics 1, 3, 5 receive directions -1, +1, -1.
    inline std::vector<float> spreadDirections (std::span<const float> spectrum)
    {
        std::vector<float> directions (spectrum.size() / 2, 0.0f);
        float largestMagnitude = 0.0f;
        for (size_t harmonic = 1; 2 * harmonic + 1 < spectrum.size(); ++harmonic)
            largestMagnitude = std::max (largestMagnitude, std::hypot (spectrum[2 * harmonic], spectrum[2 * harmonic + 1]));
        // Float FFT/trigonometry residue must not count as an extra harmonic and flip subsequent sides.
        const float activationThreshold = largestMagnitude * 1.0e-4f;
        float nextDirection = -1.0f;
        for (size_t harmonic = 1; 2 * harmonic + 1 < spectrum.size(); ++harmonic)
        {
            if (std::hypot (spectrum[2 * harmonic], spectrum[2 * harmonic + 1]) <= activationThreshold)
                continue;

            directions[harmonic] = nextDirection;
            nextDirection = -nextDirection;
        }
        return directions;
    }

    /// Places the pivot at centre and reaches either extreme four octaves away, independent of note or table length.
    /// Example: `tiltPosition (16, 8)` is +0.25; `tiltPosition (4, 8)` is -0.25.
    inline float tiltPosition (size_t harmonic, float pivotHarmonic)
    {
        const float ratio = static_cast<float> (std::max (size_t { 1 }, harmonic)) / std::max (1.0f, pivotHarmonic);
        return std::clamp (std::log2 (ratio) * 0.25f, -1.0f, 1.0f);
    }

    /// Adds source pan, alternating spread and frequency tilt, clamping only the final position.
    /// Example: zero spread/tilt leaves every harmonic at `sourcePan`.
    inline float harmonicPan (size_t harmonic, float direction, float sourcePan, float spreadAmount, float tiltAmount, float pivotHarmonic)
    {
        const float tiltOffset = tiltAmount == 0.0f ? 0.0f : tiltAmount * tiltPosition (harmonic, pivotHarmonic);
        return std::clamp (sourcePan + spreadAmount * direction + tiltOffset,
            -100.0f,
            100.0f);
    }

    /// Adds a packed spectrum with the same linear, mono-preserving pan law as colour collectors.
    /// Example: full negative spread sends the first active harmonic right and the second left.
    inline void renderStereoSpectrum (juce::AudioBuffer<float>& destination, std::span<const float> spectrum, std::span<const float> directions, float level, float sourcePan, float spreadAmount, float tiltAmount, float pivotHarmonic)
    {
        const size_t count = std::min (spectrum.size(), static_cast<size_t> (destination.getNumSamples()));
        for (size_t entry = 0; entry < count; entry += 2)
        {
            const size_t harmonic = entry / 2;
            const float position = harmonic == 0 ? sourcePan
                                                 : harmonicPan (harmonic, directions[harmonic], sourcePan, spreadAmount, tiltAmount, pivotHarmonic);
            const float rightGain = position * 0.01f + 1.0f;
            const float sample = level * spectrum[entry];
            destination.addSample (0, static_cast<int> (entry), sample * (2.0f - rightGain));
            destination.addSample (1, static_cast<int> (entry), sample * rightGain);
            if (entry + 1 >= count)
                continue;

            const float imaginary = level * spectrum[entry + 1];
            destination.addSample (0, static_cast<int> (entry + 1), imaginary * (2.0f - rightGain));
            destination.addSample (1, static_cast<int> (entry + 1), imaginary * rightGain);
        }
    }
}
