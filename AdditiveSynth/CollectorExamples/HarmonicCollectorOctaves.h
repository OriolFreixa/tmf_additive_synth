/*
  ==============================================================================

    HarmonicCollectorSine.h
    Created: 11/5/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "../AdditiveSynthHarmonicCollector.h"
namespace tmf
{
    static constexpr int maxBoundValue = 9;
    namespace HarmonicCollectorOctavesParameterIdSuffixes
    {
        inline static const string lowBound = "_LOWBOUND";
        inline static const string highBound = "_HIGHBOUND";
    }

    class HarmonicCollectorOctaves : public AdditiveSynthHarmonicCollector
    {
    public:
        void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, int tableSize) override
        {
            jassert (sampleRate > 0);
            jassert (numChannels > 0);
            std::vector<float> table = std::vector<float> (tableSize, 0);
            int count = 1;
            for (int i = 2; 2 * i < tableSize && count < this->highBound; i *= 2)
            {
                if(count >= this->lowBound)
                    table[2 * i] = 1;
                count++;
            }

            this->applyPanAndGainAndRenderToBuffer (audioBuffer, table);
        }

        void setLowAndHighBounds (int lowBound, int highBound)
        {
            this->lowBound = lowBound;
            this->highBound = highBound;
        }

    private:
        int lowBound = 1;
        int highBound = maxBoundValue;
    };

    class HarmonicCollectorOctavesManager : public HarmonicCollectorManager<HarmonicCollectorOctaves>
    {
    public:
        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (int index) override
        {
            if (index >= harmonicCollectors.size())
            {
                harmonicCollectors.resize (index + 1);
            }
            if (!harmonicCollectors[index])
            {
                auto newCollector = make_shared<HarmonicCollectorOctaves>();
                newCollector->setParams (params);
                newCollector->setLowAndHighBounds (lowBound, highBound);
                harmonicCollectors[index] = newCollector;
            }
            return dynamic_pointer_cast<AdditiveSynthHarmonicCollector> (harmonicCollectors[index]);
        }

        virtual unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters()
        {
            auto id = getId();
            auto baseResult = HarmonicCollectorManager::getAudioParameters();

            baseResult->addChild (make_unique<juce::AudioParameterInt> (juce::ParameterID { id + HarmonicCollectorOctavesParameterIdSuffixes::lowBound, 1 }, id + "Low Bound", 1, maxBoundValue, 1));
            baseResult->addChild (make_unique<juce::AudioParameterInt> (juce::ParameterID { id + HarmonicCollectorOctavesParameterIdSuffixes::highBound, 1 }, id + "High Bound", 1, maxBoundValue, maxBoundValue));

            return std::move (baseResult);
        }

        virtual vector<string> getAudioParametersIds()
        {
            return getAudioParametersIdsStatic();
        }

        static vector<string> getAudioParametersIdsStatic()
        {
            vector<string> ids = HarmonicCollectorManager::getAudioParametersIdsStatic();
            
            auto id = getIdStatic();
            ids.push_back (id + HarmonicCollectorOctavesParameterIdSuffixes::lowBound);
            ids.push_back (id + HarmonicCollectorOctavesParameterIdSuffixes::highBound);

            return ids;
        }

        virtual void parameterChanged (const juce::String& parameterID, float newValue)
        {
            if (parameterID == (String) (id + HarmonicCollectorOctavesParameterIdSuffixes::lowBound))
            {
                lowBound = newValue;

                for (auto& collector : harmonicCollectors)
                {
                    collector->setLowAndHighBounds (lowBound, highBound);
                }
            }
            else if (parameterID == (String) (id + HarmonicCollectorOctavesParameterIdSuffixes::highBound))
            {
                highBound = newValue;

                for (auto& collector : harmonicCollectors)
                {
                    collector->setLowAndHighBounds (lowBound, highBound);
                }
            }
            else
            {
                HarmonicCollectorManager::parameterChanged (parameterID, newValue);
            }
        }

    private:
        int lowBound = 1;
        int highBound = maxBoundValue;
    };
}