/*
  ==============================================================================

    ShapeSpectra.h

    Packed-spectrum builders for the triangle, saw and square collectors.
    Every builder writes harmonics in the same domain as the other collectors:
    the wavetable later weights harmonic n by 1/n, so the finished wave is the
    unit-peak shape scaled by the collector level.

  ==============================================================================
*/

#pragma once
#include "../../FFT.h"
#include "../AdditiveWavetableMath.h"
#include <cmath>
#include <span>
#include <vector>

namespace tmf::shapeSpectra
{
    /// Largest fraction of the peak that Flatten can cut away. Example: 0.95 leaves a plateau at 5 percent of the peak.
    inline constexpr float maxFlattenAmount = 0.95f;
    /// Narrowest share of a half cycle a triangle may keep at full Base. Example: 0.05 gives thin spikes.
    inline constexpr float minTriangleWidth = 0.05f;
    /// Widest edge ramp, as a share of one cycle, reached at full Edge.
    inline constexpr float maxEdgeRamp = 0.5f;

    /// Clips a unit-peak value to a lower ceiling and scales it back to unit peak.
    /// Example: `flattenTip (1.0f, 0.5f)` is 1 and `flattenTip (0.4f, 0.5f)` is about 0.76.
    inline float flattenTip (float value, float flatten)
    {
        const auto ceiling = 1.0f - maxFlattenAmount * juce::jlimit (0.0f, 1.0f, flatten);
        return juce::jlimit (-ceiling, ceiling, value) / ceiling;
    }

    /// Bends a rising edge. Positive curve sags the edge, negative curve bulges it.
    /// Example: `bendEdge (0.5f, 1.0f)` is 0.25 and `bendEdge (0.5f, -1.0f)` is about 0.71.
    inline float bendEdge (float magnitude, float curve)
    {
        return std::pow (juce::jlimit (0.0f, 1.0f, magnitude), std::exp2 (curve));
    }

    /// One point of the triangle cycle, with phase in [0, 1). It starts at zero and peaks at a quarter cycle.
    /// Example: `triangleSample (0.25f, 0, 0, 0)` is 1.
    inline float triangleSample (float phase, float flatten, float base, float curve)
    {
        const auto positiveHalf = phase < 0.5f;
        const auto halfPhase = positiveHalf ? 2.0f * phase : 2.0f * phase - 1.0f;
        const auto width = 1.0f - (1.0f - minTriangleWidth) * juce::jlimit (0.0f, 1.0f, base);
        const auto height = juce::jmax (0.0f, 1.0f - std::abs (halfPhase - 0.5f) / (0.5f * width));
        const auto bent = bendEdge (height, curve);
        return flattenTip (positiveHalf ? bent : -bent, flatten);
    }

    /// One point of the saw cycle, with phase in [0, 1). It falls from +1 to -1 and jumps back at the cycle start.
    /// Example: `sawSample (0.25f, 0, 0)` is 0.5.
    inline float sawSample (float phase, float flatten, float curve)
    {
        const auto ramp = 1.0f - 2.0f * phase;
        const auto bent = std::copysign (bendEdge (std::abs (ramp), curve), ramp);
        return flattenTip (bent, flatten);
    }

    /// Converts one sampled cycle into the collector spectrum domain using the module FFT.
    /// Example: a unit sine cycle yields an imaginary entry of -0.5 at harmonic 1.
    inline void transformCycleToSpectrum (std::span<const float> cycle, std::span<float> spectrum)
    {
        jassert (cycle.size() == static_cast<size_t> (waveTableSize));
        jassert (spectrum.size() == static_cast<size_t> (waveTableSize));
        // Forward transforms write two entries past the table, hence the padding.
        thread_local FourierTransform fourierTransform (waveTableOrder);
        thread_local std::vector<float> scratch (static_cast<size_t> (waveTableSize) + 2u);

        std::copy (cycle.begin(), cycle.end(), scratch.begin());
        fourierTransform.transformRealForward (scratch.data());
        const auto size = static_cast<float> (waveTableSize);
        for (size_t harmonic = 1; 2u * harmonic + 1u < spectrum.size(); ++harmonic)
        {
            const auto weight = static_cast<float> (harmonic) / size;
            spectrum[2u * harmonic] = weight * scratch[2u * harmonic];
            spectrum[2u * harmonic + 1u] = weight * scratch[2u * harmonic + 1u];
        }
    }

    template <class CycleFunction>
    void fillSpectrumFromCycle (std::span<float> spectrum, CycleFunction&& sampleAt)
    {
        std::vector<float> cycle (static_cast<size_t> (waveTableSize));
        for (size_t index = 0; index < cycle.size(); ++index)
            cycle[index] = sampleAt (static_cast<float> (index) / static_cast<float> (cycle.size()));

        transformCycleToSpectrum (cycle, spectrum);
    }

    /// Controls: flatten, base, curve.
    inline void fillTriangleSpectrum (std::span<const float> controls, std::span<float> spectrum)
    {
        fillSpectrumFromCycle (spectrum, [controls] (float phase) {
            return triangleSample (phase, controls[0], controls[1], controls[2]);
        });
    }

    /// Controls: flatten, curve.
    inline void fillSawSpectrum (std::span<const float> controls, std::span<float> spectrum)
    {
        fillSpectrumFromCycle (spectrum, [controls] (float phase) {
            return sawSample (phase, controls[0], controls[1]);
        });
    }

    /// Returns the box-filter attenuation of harmonic n for an edge ramp of the given cycle share.
    /// Example: `edgeAttenuation (3, 0.0f)` is 1.
    inline float edgeAttenuation (size_t harmonic, float ramp)
    {
        if (ramp <= 0.0f)
            return 1.0f;

        const auto argument = juce::MathConstants<float>::pi * static_cast<float> (harmonic) * ramp;
        return std::sin (argument) / argument;
    }

    /// Controls: width, edge. Written in closed form because a pulse has an exact spectrum.
    /// Example: width 0.5 leaves only odd harmonics.
    inline void fillSquareSpectrum (std::span<const float> controls, std::span<float> spectrum)
    {
        const auto width = controls[0];
        const auto ramp = maxEdgeRamp * juce::jlimit (0.0f, 1.0f, controls[1]);
        constexpr auto pi = juce::MathConstants<float>::pi;
        for (size_t harmonic = 1; 2u * harmonic + 1u < spectrum.size(); ++harmonic)
        {
            const auto angle = juce::MathConstants<float>::twoPi * static_cast<float> (harmonic) * width;
            const auto attenuation = edgeAttenuation (harmonic, ramp);
            spectrum[2u * harmonic] = attenuation * std::sin (angle) / pi;
            spectrum[2u * harmonic + 1u] = -attenuation * (1.0f - std::cos (angle)) / pi;
        }
    }
}
