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
#include "ControlledCollectorDefinition.h"
#include "HarmonicStereo.h"
#include <algorithm>
#include <span>
#include <string>
#include <vector>

namespace tmf
{
    class HarmonicCollectorWithControls : public AdditiveSynthHarmonicCollector
    {
    public:
        explicit HarmonicCollectorWithControls (const ControlledCollectorDefinition& definitionToUse)
            : definition (definitionToUse)
        {
            const auto controlCount = definition.controls.size() + harmonicStereo::controlCount;
            baseValues.resize (controlCount);
            modulationOffsets.assign (controlCount, 0.0f);
            effectiveValues.resize (controlCount);
            cachedValues.assign (controlCount, 0.0f);
            for (size_t controlIndex = 0; controlIndex < controlCount; ++controlIndex)
            {
                baseValues[controlIndex] = controlSpec (controlIndex).defaultValue;
                controlIds.emplace_back (controlIndex < definition.controls.size()
                                             ? definition.controlId (controlIndex)
                                             : definition.stereoControlId (controlIndex - definition.controls.size()));
            }

            updateEffectiveValues();
        }

        /// Sets the unmodulated control values in definition order. Example: `collector.setControlValues ({ 0.5f, 0.0f, 0.0f })`.
        void setControlValues (std::span<const float> values)
        {
            jassert (values.size() == definition.controls.size());
            const auto count = std::min (values.size(), definition.controls.size());
            std::copy_n (values.begin(), count, baseValues.begin());
            updateEffectiveValues();
        }

        /// Sets stereo detail values in Spread/Tilt/Pivot order. Example: `collector.setStereoControlValues ({ 50, 0, 8 })`.
        void setStereoControlValues (std::span<const float> values)
        {
            jassert (values.size() == harmonicStereo::controlCount);
            if (values.size() != harmonicStereo::controlCount)
                return;

            std::copy (values.begin(), values.end(), baseValues.begin() + static_cast<std::ptrdiff_t> (definition.controls.size()));
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
            const auto stereo = effectiveValues.data() + definition.controls.size();
            harmonicStereo::renderStereoSpectrum (audioBuffer, std::span<const float> { spectrum.data(), entryCount }, stereoDirections, level.getCurrentValue(), pan.getCurrentValue(), stereo[harmonicStereo::spread], stereo[harmonicStereo::tilt], stereo[harmonicStereo::pivot]);
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
        const CollectorControlSpec& controlSpec (size_t index) const
        {
            if (index < definition.controls.size())
                return definition.controls[index];

            return harmonicStereoControlSpecs()[index - definition.controls.size()];
        }

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
            const auto& range = controlSpec (controlIndex).range;
            modulationOffsets[controlIndex] = value * (range.end - range.start);
            updateEffectiveValues();
        }

        float normalisedPosition (size_t controlIndex) const noexcept
        {
            const auto& range = controlSpec (controlIndex).range;
            return (effectiveValues[controlIndex] - range.start) / (range.end - range.start);
        }

        void updateEffectiveValues()
        {
            for (size_t controlIndex = 0; controlIndex < effectiveValues.size(); ++controlIndex)
            {
                const auto& range = controlSpec (controlIndex).range;
                effectiveValues[controlIndex] = juce::jlimit (
                    range.start, range.end, baseValues[controlIndex] + modulationOffsets[controlIndex]);
            }
        }

        void refreshSpectrumIfNeeded()
        {
            const std::span<const float> timbreValues { effectiveValues.data(), definition.controls.size() };
            if (!spectrum.empty() && std::equal (timbreValues.begin(), timbreValues.end(), cachedValues.begin()))
                return;

            // Rebuilding can cost an FFT, so it only happens when a control actually moved.
            spectrum.assign (static_cast<size_t> (waveTableSize), 0.0f);
            definition.buildSpectrum (timbreValues, spectrum);
            stereoDirections = harmonicStereo::spreadDirections (spectrum);
            cachedValues = effectiveValues;
        }

        const ControlledCollectorDefinition& definition;
        std::vector<juce::String> controlIds;
        std::vector<float> baseValues;
        std::vector<float> modulationOffsets;
        std::vector<float> effectiveValues;
        std::vector<float> cachedValues;
        std::vector<float> spectrum;
        std::vector<float> stereoDirections;
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
                collectors[index]->setStereoControlValues (stereoValues);
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

            for (size_t index = 0; index < harmonicStereo::controlCount; ++index)
            {
                const auto& control = harmonicStereoControlSpecs()[index];
                descriptions.push_back (makeFloatSynthParameter (groupPath, definition.stereoControlId (index), definition.displayName + " " + control.name, control.range, control.defaultValue, true));
            }

            return descriptions;
        }

        void applyParameterSnapshot (std::span<const float> values) override
        {
            const size_t timbreEnd = 3 + definition.controls.size();
            jassert (values.size() == timbreEnd || values.size() == timbreEnd + harmonicStereo::controlCount);
            if (values.size() != timbreEnd && values.size() != timbreEnd + harmonicStereo::controlCount)
                return;

            params = { values[0], values[2], static_cast<int> (values[1]) };
            std::copy_n (values.begin() + 3, controlValues.size(), controlValues.begin());
            resetStereoSnapshot (values.subspan (timbreEnd));
            for (const auto& collector : collectors)
            {
                if (collector == nullptr)
                    continue;

                collector->setParams (params);
                collector->setControlValues (controlValues);
                collector->setStereoControlValues (stereoValues);
            }
        }

    private:
        void resetStereoSnapshot (std::span<const float> values)
        {
            for (size_t index = 0; index < stereoValues.size(); ++index)
                stereoValues[index] = values.empty() ? harmonicStereoControlSpecs()[index].defaultValue : values[index];
        }

        const ControlledCollectorDefinition& definition;
        AdditiveSynthHarmonicCollectorParams params;
        std::vector<float> controlValues;
        std::array<float, harmonicStereo::controlCount> stereoValues { 0.0f, 0.0f, 8.0f };
        std::vector<std::shared_ptr<HarmonicCollectorWithControls>> collectors;
    };
}
