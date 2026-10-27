/*
  ==============================================================================

    ParitySpectrum.h

    Packed-spectrum builder for the uneven and even collectors. Weights are
    written into the real entries, the same phase colours use, so parity energy
    stacks directly on top of colour harmonics.

  ==============================================================================
*/

#pragma once
#include "../AdditiveWavetableMath.h"
#include <cmath>
#include <span>

namespace tmf::paritySpectrum
{
    enum class Parity {
        uneven,
        even
    };

    /// Indices into the control values of a parity collector.
    enum Control : size_t {
        lowHarmonic,
        lowSlope,
        highHarmonic,
        highSlope,
        controlCount
    };

    /// Decibels of attenuation that one octave of slope removes. Example: a 6.02 dB slope halves the weight per octave.
    inline constexpr float decibelsPerOctaveOfHalving = 6.0206f;

    /// Returns the gain of one harmonic. Harmonics below `lowHarmonic` and above `highHarmonic` fall away at their slope.
    /// Example: `harmonicWeight (6, 4, 6.0206, 8, 6.0206)` is 1 and `harmonicWeight (16, 4, 6.0206, 8, 6.0206)` is 0.5.
    inline float harmonicWeight (
        float harmonic, float lowHarmonic, float lowSlope, float highHarmonic, float highSlope)
    {
        auto weight = 1.0f;
        if (harmonic < lowHarmonic)
            weight *= std::pow (harmonic / lowHarmonic, lowSlope / decibelsPerOctaveOfHalving);

        if (harmonic > highHarmonic)
            weight *= std::pow (highHarmonic / harmonic, highSlope / decibelsPerOctaveOfHalving);

        return weight;
    }

    /// Returns whether a harmonic belongs to the parity. Example: `belongsTo (3, Parity::uneven)` is true.
    inline bool belongsTo (size_t harmonic, Parity parity)
    {
        const auto isUneven = harmonic % 2u == 1u;
        return parity == Parity::uneven ? isUneven : !isUneven;
    }

    /// Fills the real entries of every harmonic of the chosen parity.
    /// Example: `fillParitySpectrum (controls, spectrum, Parity::even)` leaves harmonic 1 empty.
    inline void fillParitySpectrum (std::span<const float> controls, std::span<float> spectrum, Parity parity)
    {
        for (size_t harmonic = 1; 2u * harmonic + 1u < spectrum.size(); ++harmonic)
        {
            if (!belongsTo (harmonic, parity))
                continue;

            spectrum[2u * harmonic] = harmonicWeight (static_cast<float> (harmonic),
                controls[lowHarmonic],
                controls[lowSlope],
                controls[highHarmonic],
                controls[highSlope]);
        }
    }

    inline void fillUnevenSpectrum (std::span<const float> controls, std::span<float> spectrum)
    {
        fillParitySpectrum (controls, spectrum, Parity::uneven);
    }

    inline void fillEvenSpectrum (std::span<const float> controls, std::span<float> spectrum)
    {
        fillParitySpectrum (controls, spectrum, Parity::even);
    }
}
