/*
  ==============================================================================

    AdditiveSynthHarmonicCollectorManager.h
    Created: 24/6/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/
#pragma once

#include "AdditiveSynthHarmonicCollector.h"
namespace tmf
{
    class HarmonicCollectorManagerInterface : public juce::AudioProcessorValueTreeState::Listener
    {
    public:
        HarmonicCollectorManagerInterface() = default;
        virtual ~HarmonicCollectorManagerInterface() = default;
        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (int index) = 0;
        virtual string getId() const = 0;
        virtual unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() = 0;
        virtual vector<string> getAudioParametersIds() = 0;
        virtual vector<ModTargetData> getModTargetData() = 0;
    };

    template <class HC>
    class HarmonicCollectorManager : public HarmonicCollectorManagerInterface
    {
        static_assert (std::is_base_of<AdditiveSynthHarmonicCollector, HC>::value, "VI must inherit AdditiveSynthHarmonicCollector");

    public:
        HarmonicCollectorManager() = default;
        string getId() const override
        {
            return getIdStatic();
        }

        static string getIdStatic()
        {
            if (id.empty())
            {
                juce::String className = typeid (HC).name();
                className = className.replace ("class", "")
                                .replace ("tmf::", "")
                                .replace ("HarmonicCollector", "")
                                .replace (" ", "");
                id = className.toStdString();
            }

            return id;
        }

        int getOrder() { return params.order; }

        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (int index) override
        {
            if (index >= harmonicCollectors.size())
            {
                harmonicCollectors.resize (index + 1);
            }
            if (!harmonicCollectors[index])
            {
                harmonicCollectors[index] = make_shared<HC>();
                harmonicCollectors[index]->setParams (params);
                harmonicCollectors[index]->setId (getIdStatic());
            }
            return dynamic_pointer_cast<AdditiveSynthHarmonicCollector> (harmonicCollectors[index]);
        }

        virtual unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters()
        {
            auto id = getId();
            auto result = make_unique<juce::AudioProcessorParameterGroup> (id, id, "_");

            // All parameters must be prefixed by the id of the collector
            result->addChild (make_unique<juce::AudioParameterFloat> (juce::ParameterID { id + BaseParameterIdSuffixes::level, 1 }, id + "Level", juce::NormalisableRange<float> { 0, 1, 0.001, 0.65 }, 0.5));
            result->addChild (make_unique<juce::AudioParameterInt> (juce::ParameterID { id + BaseParameterIdSuffixes::order, 1 }, id + "Order", -1, 1000, -1));
            result->addChild (make_unique<juce::AudioParameterFloat> (juce::ParameterID { id + BaseParameterIdSuffixes::pan, 1 }, id + "Pan", juce::NormalisableRange<float> { -100, 100, 0.001, 1 }, 0));

            return std::move (result);
        };

        virtual vector<string> getAudioParametersIds()
        {
            return getAudioParametersIdsStatic();
        }

        static vector<string> getAudioParametersIdsStatic()
        {
            vector<string> ids;
            auto id = getIdStatic();
            ids.push_back (id + BaseParameterIdSuffixes::level);
            ids.push_back (id + BaseParameterIdSuffixes::order);
            ids.push_back (id + BaseParameterIdSuffixes::pan);

            return ids;
        }

        virtual void parameterChanged (const juce::String& parameterID, float newValue)
        {
            if (parameterID == (String) (id + BaseParameterIdSuffixes::level))
            {
                params.level = newValue;
            }
            else if (parameterID == (String) (id + BaseParameterIdSuffixes::order))
            {
                params.order = newValue;
            }
            else if (parameterID == (String) (id + BaseParameterIdSuffixes::pan))
            {
                params.pan = newValue;
            }

            for (auto& collector : harmonicCollectors)
            {
                collector->setParams (params);
            }
        }

        virtual vector<ModTargetData> getModTargetData()
        {
            auto params = getAudioParameters();
            vector<ModTargetData> modTargets;
            for (auto& param : params->getParameters (false))
            {
                ModTargetData data;
                auto paramWithId = static_cast<juce::AudioProcessorParameterWithID*> (param);

                data.id = paramWithId->paramID;
                data.name = paramWithId->getName (INT_MAX);
                modTargets.push_back (data);
            }

            return modTargets;
        }

    protected:
        inline static string id = "";
        vector<shared_ptr<HC>> harmonicCollectors;
        AdditiveSynthHarmonicCollectorParams params;
        int currentNote = -1;
    };
}
