/*
  ==============================================================================

    HarmonicCollectorOctaveHarmonics.h
    Created: 4/7/2026
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "HarmonicCollectorOctaves.h"
#include <array>

namespace tmf
{
    namespace HarmonicCollectorOctaveHarmonicsParameterIdSuffixes
    {
        inline std::string getLevel (int harmonicIndex)
        {
            return "_H" + std::to_string (harmonicIndex + 1) + "_LVL";
        }

        inline std::string getPan (int harmonicIndex)
        {
            return "_HARMONIC" + std::to_string (harmonicIndex + 1) + "_PAN";
        }
    }

    struct HarmonicCollectorOctaveHarmonicsParams
    {
        HarmonicCollectorOctaveHarmonicsParams()
        {
            levels.fill (1.0f);
            pans.fill (0.0f);
        }

        std::array<float, maxBoundValue> levels;
        std::array<float, maxBoundValue> pans;
    };

    class HarmonicCollectorOctaveHarmonics : public AdditiveSynthHarmonicCollector
    {
    public:
        HarmonicCollectorOctaveHarmonics (int startIndex = 1)
            : startHarmonic (getHarmonicCollectorOctavesStartHarmonic (startIndex))
        {
            harmonicModValues.levels.fill (0.0f);
            doUpdateHarmonicParameters();
        }

        void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, size_t tableSize) override
        {
            jassert (sampleRate > 0);
            jassert (numChannels > 0);
            jassert (audioBuffer.getNumChannels() >= 2);

            if (tableSize <= 2)
                return;

            auto leftTable = std::vector<float> (tableSize, 0.0f);
            auto rightTable = std::vector<float> (tableSize, 0.0f);
            auto harmonicNumber = static_cast<size_t> (startHarmonic);

            for (int harmonicIndex = 0; harmonicIndex < maxBoundValue && 2u * harmonicNumber < tableSize; ++harmonicIndex)
            {
                const auto levelValue = getHarmonicLevel (harmonicIndex) * level.getCurrentValue();
                const auto panValue = juce::jlimit (-100.0f, 100.0f, getHarmonicPan (harmonicIndex) + pan.getCurrentValue());
                const auto normalisedPan = (panValue * 0.005f) + 0.5f;
                const auto leftValue = (1.0f - normalisedPan) * 2.0f;
                const auto rightValue = normalisedPan * 2.0f;
                const auto realIndex = 2u * harmonicNumber;

                leftTable[realIndex] = levelValue * leftValue;
                rightTable[realIndex] = levelValue * rightValue;
                harmonicNumber *= 2u;
            }

            juce::FloatVectorOperations::add (audioBuffer.getWritePointer (0), leftTable.data(), static_cast<int> (tableSize));
            juce::FloatVectorOperations::add (audioBuffer.getWritePointer (1), rightTable.data(), static_cast<int> (tableSize));
        }

        void setHarmonicParams (HarmonicCollectorOctaveHarmonicsParams newParams)
        {
            harmonicParamsValues = newParams;
            doUpdateHarmonicParameters();
        }

        bool updateModTargetValue (juce::String parameterID, float value) override
        {
            jassert (id != "");
            if (! parameterID.startsWith (id))
                return false;

            for (int harmonicIndex = 0; harmonicIndex < maxBoundValue; ++harmonicIndex)
            {
                if (parameterID.endsWith (HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getLevel (harmonicIndex)))
                {
                    harmonicModValues.levels[static_cast<size_t> (harmonicIndex)] = value;
                    doUpdateHarmonicParameters();
                    return true;
                }

                if (parameterID.endsWith (HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getPan (harmonicIndex)))
                {
                    harmonicModValues.pans[static_cast<size_t> (harmonicIndex)] = juce::jmap (value, -1.0f, 1.0f, -200.0f, 200.0f);
                    doUpdateHarmonicParameters();
                    return true;
                }
            }

            return AdditiveSynthHarmonicCollector::updateModTargetValue (parameterID, value);
        }

        float getHarmonicLevel (int harmonicIndex) const
        {
            return harmonicLevels[static_cast<size_t> (juce::jlimit (0, maxBoundValue - 1, harmonicIndex))];
        }

        float getHarmonicPan (int harmonicIndex) const
        {
            return harmonicPans[static_cast<size_t> (juce::jlimit (0, maxBoundValue - 1, harmonicIndex))];
        }

    private:
        void doUpdateHarmonicParameters()
        {
            for (size_t i = 0; i < harmonicLevels.size(); ++i)
            {
                harmonicLevels[i] = juce::jlimit (0.0f, 1.0f, harmonicModValues.levels[i] + harmonicParamsValues.levels[i]);
                harmonicPans[i] = juce::jlimit (-100.0f, 100.0f, harmonicModValues.pans[i] + harmonicParamsValues.pans[i]);
            }
        }

        HarmonicCollectorOctaveHarmonicsParams harmonicParamsValues;
        HarmonicCollectorOctaveHarmonicsParams harmonicModValues;
        std::array<float, maxBoundValue> harmonicLevels {};
        std::array<float, maxBoundValue> harmonicPans {};
        int startHarmonic = 1;
    };

    class HarmonicCollectorOctaveHarmonicsManager : public HarmonicCollectorManager<HarmonicCollectorOctaveHarmonics>
    {
    public:
        HarmonicCollectorOctaveHarmonicsManager (int newStartIndex = 1)
            : startIndex (juce::jmax (1, newStartIndex))
        {
        }

        string getId() const override
        {
            return getTypeId() + std::to_string (startIndex - 1);
        }

        shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (size_t index) override
        {
            if (index >= harmonicCollectors.size())
                harmonicCollectors.resize (index + 1);

            if (! harmonicCollectors[index])
            {
                auto newCollector = make_shared<HarmonicCollectorOctaveHarmonics> (startIndex);
                newCollector->setParams (params);
                newCollector->setId (getId());
                newCollector->setHarmonicParams (harmonicParams);
                harmonicCollectors[index] = newCollector;
            }

            return dynamic_pointer_cast<AdditiveSynthHarmonicCollector> (harmonicCollectors[index]);
        }

        unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() override
        {
            auto id = getId();
            auto result = HarmonicCollectorManager::getAudioParameters();

            for (int harmonicIndex = 0; harmonicIndex < maxBoundValue; ++harmonicIndex)
            {
                const auto harmonicNumber = getHarmonicNumber (harmonicIndex);
                result->addChild (make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { id + HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getLevel (harmonicIndex), 1 },
                    id + "Harmonic " + std::to_string (harmonicNumber) + " Level",
                    juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f, 0.65f },
                    harmonicParams.levels[static_cast<size_t> (harmonicIndex)]));
                result->addChild (make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { id + HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getPan (harmonicIndex), 1 },
                    id + "Harmonic " + std::to_string (harmonicNumber) + " Pan",
                    juce::NormalisableRange<float> { -100.0f, 100.0f, 0.001f, 1.0f },
                    harmonicParams.pans[static_cast<size_t> (harmonicIndex)]));
            }

            return result;
        }

        vector<string> getAudioParametersIds() override
        {
            return getAudioParametersIdsStatic (startIndex - 1);
        }

        static vector<string> getAudioParametersIdsStatic (int instanceNumber)
        {
            const auto id = getTypeId() + std::to_string (juce::jmax (0, instanceNumber));
            vector<string> ids {
                id + BaseParameterIdSuffixes::level,
                id + BaseParameterIdSuffixes::order,
                id + BaseParameterIdSuffixes::pan
            };

            for (int harmonicIndex = 0; harmonicIndex < maxBoundValue; ++harmonicIndex)
            {
                ids.push_back (id + HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getLevel (harmonicIndex));
                ids.push_back (id + HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getPan (harmonicIndex));
            }

            return ids;
        }

        static int getHarmonicNumberStatic (int instanceNumber, int harmonicIndex)
        {
            auto harmonicNumber = static_cast<size_t> (getHarmonicCollectorOctavesStartHarmonic (instanceNumber + 1));
            for (int i = 0; i < harmonicIndex; ++i)
                harmonicNumber *= 2u;

            return static_cast<int> (harmonicNumber);
        }

        void parameterChanged (const juce::String& parameterID, float newValue) override
        {
            const auto collectorId = getId();

            if (parameterID == (String) (collectorId + BaseParameterIdSuffixes::level))
                params.level = newValue;
            else if (parameterID == (String) (collectorId + BaseParameterIdSuffixes::order))
                params.order = static_cast<int> (newValue);
            else if (parameterID == (String) (collectorId + BaseParameterIdSuffixes::pan))
                params.pan = newValue;
            else
                updateHarmonicParameter (parameterID, newValue);

            for (auto& collector : harmonicCollectors)
            {
                if (! collector)
                    continue;

                collector->setParams (params);
                collector->setHarmonicParams (harmonicParams);
            }
        }

    private:
        void updateHarmonicParameter (const juce::String& parameterID, float newValue)
        {
            const auto collectorId = getId();

            for (int harmonicIndex = 0; harmonicIndex < maxBoundValue; ++harmonicIndex)
            {
                if (parameterID == (String) (collectorId + HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getLevel (harmonicIndex)))
                {
                    harmonicParams.levels[static_cast<size_t> (harmonicIndex)] = newValue;
                    return;
                }

                if (parameterID == (String) (collectorId + HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getPan (harmonicIndex)))
                {
                    harmonicParams.pans[static_cast<size_t> (harmonicIndex)] = newValue;
                    return;
                }
            }
        }

        int getHarmonicNumber (int harmonicIndex) const
        {
            return getHarmonicNumberStatic (startIndex - 1, harmonicIndex);
        }

        static string getTypeId()
        {
            return "OctHarm";
        }

        HarmonicCollectorOctaveHarmonicsParams harmonicParams;
        int startIndex = 1;
    };
}
