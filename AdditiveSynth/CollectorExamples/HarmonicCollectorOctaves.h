/*
  ==============================================================================

    HarmonicCollectorSine.h
    Created: 11/5/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "../AdditiveSynthHarmonicCollectorManager.h"
namespace tmf
{
    static constexpr int maxBoundValue = 9;
    namespace HarmonicCollectorOctavesParameterIdSuffixes
    {
        inline static const string lowBound = "_LOWBOUND";
        inline static const string highBound = "_HIGHBOUND";
    }

    struct HarmonicCollectorOctavesParams
    {
        int lowBound = 1;
        int highBound = maxBoundValue;
    };

    class HarmonicCollectorOctaves : public AdditiveSynthHarmonicCollector
    {
    public:
        HarmonicCollectorOctaves (int startIndex = 1)
        {
            if (startIndex <= 1)
                startHarmonic = 1;
            else
                startHarmonic = 2 * (startIndex - 1);
        }

        void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, size_t tableSize) override
        {
            jassert (sampleRate > 0);
            jassert (numChannels > 0);
            std::vector<float> table = std::vector<float> (tableSize, 0.0f);
            int count = 1;

            // Start at the configured harmonic and add its octave chain (harmonic * 2^n)
            size_t harmonicNumber = static_cast<size_t> (startHarmonic);
            while (2u * harmonicNumber < tableSize && count < this->highBound)
            {
                if (count >= this->lowBound)
                    table[2u * harmonicNumber] = 1.0f;
                harmonicNumber *= 2; // next octave of the current harmonic
                count++;
            }

            this->applyPanAndGainAndRenderToBuffer (audioBuffer, table);
        }

        void setParamsValues (HarmonicCollectorOctavesParams pValues)
        {
            octaveParamsValues = pValues;
            doUpdateOctaveParameters();
        }

        bool updateModTargetValue (juce::String parameterID, float value) override
        {
            jassert (id != "");
            if (!parameterID.startsWith (id))
            {
                return false; // Not a parameter of this collector
            }

            // Optimized for: if (parameterID == (String) (id + HarmonicCollectorOctavesParameterIdSuffixes::lowBound))
            if (parameterID.endsWith (HarmonicCollectorOctavesParameterIdSuffixes::lowBound))
            {
                octaveModValues.lowBound = static_cast<int> (juce::jmap (value, -1.0f, 1.0f, static_cast<float> (-maxBoundValue), static_cast<float> (maxBoundValue)));
            }
            else if (parameterID.endsWith (HarmonicCollectorOctavesParameterIdSuffixes::highBound))
            {
                octaveModValues.highBound = static_cast<int> (juce::jmap (value, -1.0f, 1.0f, static_cast<float> (-maxBoundValue), static_cast<float> (maxBoundValue)));
            }
            else
            {
                return AdditiveSynthHarmonicCollector::updateModTargetValue (parameterID, value);
            }

            doUpdateOctaveParameters();
            return true;
        }

    private:
        void doUpdateOctaveParameters()
        {
            lowBound = juce::jlimit (1, maxBoundValue, octaveModValues.lowBound + octaveParamsValues.lowBound);
            highBound = juce::jlimit (1, maxBoundValue, octaveModValues.highBound + octaveParamsValues.highBound);
        }

        HarmonicCollectorOctavesParams octaveParamsValues;
        HarmonicCollectorOctavesParams octaveModValues = { 0, 0 };
        int lowBound = 1;
        int highBound = maxBoundValue;
        int startHarmonic = 1;
    };

    class HarmonicCollectorOctavesManager : public HarmonicCollectorManager<HarmonicCollectorOctaves>
    {
    public:
        HarmonicCollectorOctavesManager (int newStartIndex = 1)
            : startIndex (newStartIndex)
        {
        }
    public:
        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (size_t index) override
        {
            if (index >= harmonicCollectors.size())
            {
                harmonicCollectors.resize (index + 1);
            }
            if (!harmonicCollectors[index])
            {
                auto newCollector = make_shared<HarmonicCollectorOctaves>(startIndex);
                newCollector->setParams (params);
                newCollector->setId (getId());
                newCollector->setParamsValues (paramsOctaves);
                harmonicCollectors[index] = newCollector;
            }
            return dynamic_pointer_cast<AdditiveSynthHarmonicCollector> (harmonicCollectors[index]);
        }

        virtual unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() override
        {
            auto id = getId();
            auto baseResult = HarmonicCollectorManager::getAudioParameters();

            baseResult->addChild (make_unique<juce::AudioParameterInt> (juce::ParameterID { id + HarmonicCollectorOctavesParameterIdSuffixes::lowBound, 1 }, id + "Low Bound", 1, maxBoundValue, 1));
            baseResult->addChild (make_unique<juce::AudioParameterInt> (juce::ParameterID { id + HarmonicCollectorOctavesParameterIdSuffixes::highBound, 1 }, id + "High Bound", 1, maxBoundValue, maxBoundValue));

            return baseResult;
        }

        virtual vector<string> getAudioParametersIds() override
        {
            auto ids = HarmonicCollectorManager::getAudioParametersIds();

            ids.push_back (getId() + HarmonicCollectorOctavesParameterIdSuffixes::lowBound);
            ids.push_back (getId() + HarmonicCollectorOctavesParameterIdSuffixes::highBound);

            return ids;
        }

        static vector<string> getAudioParametersIdsStatic (int instanceNumber)
        {
            vector<string> ids = HarmonicCollectorManager::getAudioParametersIdsStatic (instanceNumber);

            auto instanceUniqueId = getTypeIdStatic() + std::to_string (instanceNumber);
            ids.push_back (instanceUniqueId + HarmonicCollectorOctavesParameterIdSuffixes::lowBound);
            ids.push_back (instanceUniqueId + HarmonicCollectorOctavesParameterIdSuffixes::highBound);

            return ids;
        }

        virtual void parameterChanged (const juce::String& parameterID, float newValue) override
        {
            if (parameterID == (String) (getId() + HarmonicCollectorOctavesParameterIdSuffixes::lowBound))
            {
                paramsOctaves.lowBound = static_cast<int> (newValue);

                for (auto& collector : harmonicCollectors)
                {
                    collector->setParamsValues (paramsOctaves);
                }
            }
            else if (parameterID == (String) (getId() + HarmonicCollectorOctavesParameterIdSuffixes::highBound))
            {
                paramsOctaves.highBound = static_cast<int> (newValue);

                for (auto& collector : harmonicCollectors)
                {
                    collector->setParamsValues (paramsOctaves);
                }
            }
            else
            {
                HarmonicCollectorManager::parameterChanged (parameterID, newValue);
            }
        }

    private:
        HarmonicCollectorOctavesParams paramsOctaves;
        int startIndex = 1;
    };
}
