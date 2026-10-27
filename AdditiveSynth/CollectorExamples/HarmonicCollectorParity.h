/*
  ==============================================================================

    HarmonicCollectorParity.h

    Uneven and even collectors. They share one set of controls and differ only in
    which harmonics they write; the weights live in ParitySpectrum.h.

  ==============================================================================
*/

#pragma once
#include "../HarmonicCollectorWithControls.h"
#include "ParitySpectrum.h"

namespace tmf
{
    inline std::vector<CollectorControlSpec> parityControlSpecs()
    {
        constexpr auto highest = static_cast<float> (waveTableSize / 4);
        return {
            { "_LOWH", "Low H", { 1.0f, highest, 0.01f, 0.4f }, 1.0f },
            { "_LOWSLOPE", "Low Slope", { 0.0f, 96.0f, 0.1f }, 12.0f },
            { "_HIGHH", "High H", { 1.0f, highest, 0.01f, 0.4f }, 32.0f },
            { "_HIGHSLOPE", "High Slope", { 0.0f, 96.0f, 0.1f }, 12.0f },
        };
    }

    inline SynthParameterGroupDescription parityFamilyGroup()
    {
        return { "PARITY", "Parity", "_" };
    }

    /// Uneven harmonics with Low H/Low Slope and High H/High Slope shaping.
    /// Example: `HarmonicCollectorWithControlsManager manager { unevenCollectorDefinition() }`.
    inline const ControlledCollectorDefinition& unevenCollectorDefinition()
    {
        static const ControlledCollectorDefinition definition {
            "ParityUneven",
            "Uneven",
            parityFamilyGroup(),
            parityControlSpecs(),
            &paritySpectrum::fillUnevenSpectrum
        };

        return definition;
    }

    /// Even harmonics with the same controls as the uneven collector.
    /// Example: `HarmonicCollectorWithControlsManager manager { evenCollectorDefinition() }`.
    inline const ControlledCollectorDefinition& evenCollectorDefinition()
    {
        static const ControlledCollectorDefinition definition {
            "ParityEven",
            "Even",
            parityFamilyGroup(),
            parityControlSpecs(),
            &paritySpectrum::fillEvenSpectrum
        };

        return definition;
    }
}
