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
        void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, size_t tableSize) override
        {
            jassert (sampleRate > 0);
            jassert (numChannels > 0);
            std::vector<float> table = std::vector<float> (tableSize, 0);
            int count = 1;
            for (size_t i = 2; 2 * i < tableSize && count < this->highBound; i *= 2)
            {
                if (count >= this->lowBound)
                    table[2u * i] = 1;
                count++;
            }

            this->applyPanAndGainAndRenderToBuffer (audioBuffer, table);
        }

        void setParamsValues (HarmonicCollectorOctavesParams pValues)
        {
            paramsValues = pValues;
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
                modValues.lowBound = juce::jmap (value, -1.f, 1.f, (float) -maxBoundValue, (float) maxBoundValue);
            }
            else if (parameterID.endsWith (HarmonicCollectorOctavesParameterIdSuffixes::highBound))
            {
                modValues.highBound = juce::jmap (value, -1.f, 1.f, (float) -maxBoundValue, (float) maxBoundValue);
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
            lowBound = juce::jlimit (1, maxBoundValue, modValues.lowBound + paramsValues.lowBound);
            highBound = juce::jlimit (1, maxBoundValue, modValues.highBound + paramsValues.highBound);
        }

        HarmonicCollectorOctavesParams paramsValues;
        HarmonicCollectorOctavesParams modValues = { 0, 0 };
        int lowBound = 1;
        int highBound = maxBoundValue;
    };

    class HarmonicCollectorOctavesManager : public HarmonicCollectorManager<HarmonicCollectorOctaves>
    {
    public:
        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (size_t index) override
        {
            if (index >= harmonicCollectors.size())
            {
                harmonicCollectors.resize (index + 1);
            }
            if (!harmonicCollectors[index])
            {
                auto newCollector = make_shared<HarmonicCollectorOctaves>();
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
                paramsOctaves.lowBound = newValue;

                for (auto& collector : harmonicCollectors)
                {
                    collector->setParamsValues (paramsOctaves);
                }
            }
            else if (parameterID == (String) (getId() + HarmonicCollectorOctavesParameterIdSuffixes::highBound))
            {
                paramsOctaves.highBound = newValue;

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
    };
}
