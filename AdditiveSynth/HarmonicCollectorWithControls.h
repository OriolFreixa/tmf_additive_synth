/*
  ==============================================================================

    HarmonicCollectorWithControls.h

    A collector whose spectrum is a pure function of a few named controls, plus
    the manager that exposes those controls as host and modulation parameters.
    Shapes and parity collectors are defined as data on top of this pair.

  ==============================================================================
*/

#pragma once
#include "AdditiveSynthHarmonicCollectorManager.h"
#include "AdditiveWavetableMath.h"
#include <algorithm>
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

        /// Returns the parameter ID of one control. Example: `definition.controlId (0)` is "ShapeTriangle_FLATTEN".
        std::string controlId (size_t controlIndex) const
        {
            jassert (controlIndex < controls.size());
            return id + controls[controlIndex].suffix;
        }

        /// Returns every parameter ID in manager order: level, order, pan, then the controls.
        std::vector<std::string> allParameterIds() const
        {
            std::vector<std::string> ids { levelId(), orderId(), panId() };
            for (size_t controlIndex = 0; controlIndex < controls.size(); ++controlIndex)
                ids.push_back (controlId (controlIndex));

            return ids;
        }
    };

    class HarmonicCollectorWithControls : public AdditiveSynthHarmonicCollector
    {
    public:
        explicit HarmonicCollectorWithControls (const ControlledCollectorDefinition& definitionToUse)
            : definition (definitionToUse)
        {
            const auto controlCount = definition.controls.size();
            baseValues.resize (controlCount);
            modulationOffsets.assign (controlCount, 0.0f);
            effectiveValues.resize (controlCount);
            cachedValues.assign (controlCount, 0.0f);
            for (size_t controlIndex = 0; controlIndex < controlCount; ++controlIndex)
            {
                baseValues[controlIndex] = definition.controls[controlIndex].defaultValue;
                controlIds.emplace_back (definition.controlId (controlIndex));
            }

            updateEffectiveValues();
        }

        /// Sets the unmodulated control values in definition order. Example: `collector.setControlValues ({ 0.5f, 0.0f, 0.0f })`.
        void setControlValues (std::span<const float> values)
        {
            jassert (values.size() == baseValues.size());
            const auto count = std::min (values.size(), baseValues.size());
            std::copy_n (values.begin(), count, baseValues.begin());
            updateEffectiveValues();
        }

        /// Returns the control value after modulation and clamping. Example: `collector.getControlValue (1)`.
        float getControlValue (size_t controlIndex) const
        {
            jassert (controlIndex < effectiveValues.size());
            return effectiveValues[std::min (controlIndex, effectiveValues.size() - 1u)];
        }

        void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, size_t tableSize) override
        {
            jassert (audioBuffer.getNumChannels() >= 2);
            if (tableSize <= 2)
                return;

            refreshSpectrumIfNeeded();
            const auto entryCount = std::min (tableSize, spectrum.size());
            applyPanAndGainAndRenderToBuffer (
                audioBuffer,
                std::vector<float> (spectrum.begin(), spectrum.begin() + static_cast<std::ptrdiff_t> (entryCount)));
        }

        std::optional<ModulationTargetBinding> bindModulationTarget (const juce::String& parameterId) override
        {
            const auto controlIndex = findControl (parameterId);
            if (!controlIndex)
                return AdditiveSynthHarmonicCollector::bindModulationTarget (parameterId);

            const auto index = *controlIndex;
            return ModulationTargetBinding {
                [this, index] (float value) { setModulation (index, value); },
                [this, index] { return std::optional<float> { normalisedPosition (index) }; }
            };
        }

        bool updateModTargetValue (juce::String parameterId, float value) override
        {
            const auto controlIndex = findControl (parameterId);
            if (!controlIndex)
                return AdditiveSynthHarmonicCollector::updateModTargetValue (parameterId, value);

            setModulation (*controlIndex, value);
            return true;
        }

        std::optional<float> getEffectiveModulationTargetPosition (
            const juce::String& parameterId) const noexcept override
        {
            const auto controlIndex = findControl (parameterId);
            if (!controlIndex)
                return AdditiveSynthHarmonicCollector::getEffectiveModulationTargetPosition (parameterId);

            return normalisedPosition (*controlIndex);
        }

    private:
        std::optional<size_t> findControl (const juce::String& parameterId) const noexcept
        {
            for (size_t controlIndex = 0; controlIndex < controlIds.size(); ++controlIndex)
                if (parameterId == controlIds[controlIndex])
                    return controlIndex;

            return std::nullopt;
        }

        // Modulation arrives as a bipolar fraction of the control's span, like the base level, pan and order targets.
        void setModulation (size_t controlIndex, float value)
        {
            const auto& range = definition.controls[controlIndex].range;
            modulationOffsets[controlIndex] = value * (range.end - range.start);
            updateEffectiveValues();
        }

        float normalisedPosition (size_t controlIndex) const noexcept
        {
            const auto& range = definition.controls[controlIndex].range;
            return (effectiveValues[controlIndex] - range.start) / (range.end - range.start);
        }

        void updateEffectiveValues()
        {
            for (size_t controlIndex = 0; controlIndex < effectiveValues.size(); ++controlIndex)
            {
                const auto& range = definition.controls[controlIndex].range;
                effectiveValues[controlIndex] = juce::jlimit (
                    range.start, range.end, baseValues[controlIndex] + modulationOffsets[controlIndex]);
            }
        }

        void refreshSpectrumIfNeeded()
        {
            if (!spectrum.empty() && effectiveValues == cachedValues)
                return;

            // Rebuilding can cost an FFT, so it only happens when a control actually moved.
            spectrum.assign (static_cast<size_t> (waveTableSize), 0.0f);
            definition.buildSpectrum (effectiveValues, spectrum);
            cachedValues = effectiveValues;
        }

        const ControlledCollectorDefinition& definition;
        std::vector<juce::String> controlIds;
        std::vector<float> baseValues;
        std::vector<float> modulationOffsets;
        std::vector<float> effectiveValues;
        std::vector<float> cachedValues;
        std::vector<float> spectrum;
    };

    class HarmonicCollectorWithControlsManager : public HarmonicCollectorManagerInterface
    {
    public:
        explicit HarmonicCollectorWithControlsManager (const ControlledCollectorDefinition& definitionToUse)
            : definition (definitionToUse)
        {
            controlValues.reserve (definition.controls.size());
            for (const auto& control : definition.controls)
                controlValues.push_back (control.defaultValue);
        }

        std::string getId() const override { return definition.id; }

        const ControlledCollectorDefinition& getDefinition() const { return definition; }

        std::shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (size_t index) override
        {
            if (index >= collectors.size())
                collectors.resize (index + 1);

            if (!collectors[index])
            {
                collectors[index] = std::make_shared<HarmonicCollectorWithControls> (definition);
                collectors[index]->setParams (params);
                collectors[index]->setId (definition.id);
                collectors[index]->setControlValues (controlValues);
            }

            return collectors[index];
        }

        std::vector<SynthParameterDescription> getParameterDescriptions() override
        {
            const SynthParameterGroupPath groupPath { definition.family, { definition.id, definition.displayName, "_" } };
            std::vector<SynthParameterDescription> descriptions {
                makeFloatSynthParameter (groupPath, definition.levelId(), definition.displayName + " Level", { 0.0f, 1.0f, 0.001f, 0.65f }, params.level, true),
                makeIntSynthParameter (groupPath, definition.orderId(), definition.displayName + " Order", -1, 1000, params.order, true),
                makeFloatSynthParameter (groupPath, definition.panId(), definition.displayName + " Pan", { -100.0f, 100.0f, 0.001f, 1.0f }, params.pan, true)
            };

            for (size_t controlIndex = 0; controlIndex < definition.controls.size(); ++controlIndex)
            {
                const auto& control = definition.controls[controlIndex];
                descriptions.push_back (makeFloatSynthParameter (
                    groupPath, definition.controlId (controlIndex), definition.displayName + " " + control.name, control.range, control.defaultValue, true));
            }

            return descriptions;
        }

        void applyParameterSnapshot (std::span<const float> values) override
        {
            jassert (values.size() == 3 + definition.controls.size());
            if (values.size() != 3 + definition.controls.size())
                return;

            params = { values[0], values[2], static_cast<int> (values[1]) };
            std::copy (values.begin() + 3, values.end(), controlValues.begin());
            for (const auto& collector : collectors)
            {
                if (collector == nullptr)
                    continue;

                collector->setParams (params);
                collector->setControlValues (controlValues);
            }
        }

    private:
        const ControlledCollectorDefinition& definition;
        AdditiveSynthHarmonicCollectorParams params;
        std::vector<float> controlValues;
        std::vector<std::shared_ptr<HarmonicCollectorWithControls>> collectors;
    };
}
