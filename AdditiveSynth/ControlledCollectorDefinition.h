#pragma once

#include "AdditiveSynthHarmonicCollector.h"
#include "AdditiveWavetableMath.h"
#include "tmf_intercept_synth/synth/SynthParameterDescription.h"
#include <array>
#include <span>
#include <string>
#include <vector>

namespace tmf
{
    /// One extra control of a collector. Example: `{ "_FLATTEN", "Flatten", { 0, 1, 0.001f }, 0.0f }`.
    struct CollectorControlSpec
    {
        std::string suffix;
        juce::String name;
        juce::NormalisableRange<float> range;
        float defaultValue = 0.0f;
    };

    namespace harmonicStereo
    {
        enum Control : size_t { spread,
            tilt,
            pivot,
            controlCount };
    }

    /// Shared stereo controls of shapes and parity; zero spread/tilt preserve the original mono spectrum.
    /// Example: `harmonicStereoControlSpecs()[harmonicStereo::pivot]` describes a harmonic-number pivot.
    inline const std::array<CollectorControlSpec, harmonicStereo::controlCount>& harmonicStereoControlSpecs()
    {
        static const std::array<CollectorControlSpec, harmonicStereo::controlCount> specs { { { "_SPREAD", "Spread", { -100.0f, 100.0f, 0.01f }, 0.0f },
            { "_TILT", "Tilt", { -100.0f, 100.0f, 0.01f }, 0.0f },
            { "_PIVOT", "Pivot", { 1.0f, static_cast<float> (waveTableSize / 4), 0.01f, 0.4f }, 8.0f } } };
        return specs;
    }

    /// Writes a packed spectrum for the given control values (same order as `controls`).
    /// The target span is zeroed beforehand and spans `waveTableSize` entries.
    using CollectorSpectrumBuilder = void (*) (std::span<const float> controlValues, std::span<float> spectrum);

    /// Static description of one collector kind. Instances must outlive every collector built from them.
    /// Example: `triangleCollectorDefinition()` returns an immortal definition with three controls.
    struct ControlledCollectorDefinition
    {
        std::string id;
        juce::String displayName;
        SynthParameterGroupDescription family;
        std::vector<CollectorControlSpec> controls;
        CollectorSpectrumBuilder buildSpectrum = nullptr;

        std::string levelId() const { return id + BaseParameterIdSuffixes::level; }
        std::string orderId() const { return id + BaseParameterIdSuffixes::order; }
        std::string panId() const { return id + BaseParameterIdSuffixes::pan; }

        /// Stable identity of a stereo detail control. Example: `definition.stereoControlId (0)` is "ShapeTriangle_SPREAD".
        std::string stereoControlId (size_t index) const
        {
            jassert (index < harmonicStereoControlSpecs().size());
            return id + harmonicStereoControlSpecs()[index].suffix;
        }

        /// Returns the parameter ID of one control. Example: `definition.controlId (0)` is "ShapeTriangle_FLATTEN".
        std::string controlId (size_t controlIndex) const
        {
            jassert (controlIndex < controls.size());
            return id + controls[controlIndex].suffix;
        }

        /// Returns every parameter ID in manager order: level, order, pan, timbre controls, then stereo controls.
        std::vector<std::string> allParameterIds() const
        {
            std::vector<std::string> ids { levelId(), orderId(), panId() };
            for (size_t controlIndex = 0; controlIndex < controls.size(); ++controlIndex)
                ids.push_back (controlId (controlIndex));

            for (size_t index = 0; index < harmonicStereoControlSpecs().size(); ++index)
                ids.push_back (stereoControlId (index));

            return ids;
        }
    };

}
