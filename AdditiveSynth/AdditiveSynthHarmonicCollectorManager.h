/*
  ==============================================================================

    AdditiveSynthHarmonicCollectorManager.h
    Created: 24/6/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/
#pragma once
#include "AdditiveSynthHarmonicCollector.h"
#include "tmf_intercept_synth/synth/Interceptors/VoiceInterceptorWithModTargets.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <string>

namespace tmf
{
    class HarmonicCollectorManagerInterface : public juce::AudioProcessorValueTreeState::Listener
    {
    public:
        HarmonicCollectorManagerInterface() = default;
        virtual ~HarmonicCollectorManagerInterface() = default;
        virtual std::shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (size_t index) = 0;
        virtual std::string getId() const = 0;
        virtual std::unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() = 0;
        virtual std::vector<std::string> getAudioParametersIds() = 0;
        virtual vector<ModTargetData> getModTargetData() = 0;
    };

    template <class HC>
    class HarmonicCollectorManager : public HarmonicCollectorManagerInterface
    {
        static_assert (std::is_base_of<AdditiveSynthHarmonicCollector, HC>::value, "VI must inherit AdditiveSynthHarmonicCollector");

    public:
        HarmonicCollectorManager()
        {
            uniqueIdIndex = uniqueIdCount;
            uniqueId = getTypeIdStatic() + std::to_string (uniqueIdCount);
            uniqueIdCount++;
        }

        string getId() const override
        {
            return uniqueId;
        }

        int getOrder() { return params.order; }

        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (size_t index) override
        {
            if (index >= harmonicCollectors.size())
            {
                harmonicCollectors.resize (index + 1);
            }
            if (!harmonicCollectors[index])
            {
                harmonicCollectors[index] = make_shared<HC>();
                harmonicCollectors[index]->setParams (params);
                harmonicCollectors[index]->setId (uniqueId);
            }
            return dynamic_pointer_cast<AdditiveSynthHarmonicCollector> (harmonicCollectors[index]);
        }

        virtual unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() override
        {
            auto id = getId();
            auto displayName = getDisplayName();
            auto result = make_unique<juce::AudioProcessorParameterGroup> (id, displayName, "_");

            // All parameters must be prefixed by the id of the collector
            result->addChild (make_unique<juce::AudioParameterFloat> (juce::ParameterID { id + BaseParameterIdSuffixes::level, 1 }, displayName + " Level", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f, 0.65f }, 0.5f));
            result->addChild (make_unique<juce::AudioParameterInt> (juce::ParameterID { id + BaseParameterIdSuffixes::order, 1 }, displayName + " Order", -1, 1000, -1));
            result->addChild (make_unique<juce::AudioParameterFloat> (juce::ParameterID { id + BaseParameterIdSuffixes::pan, 1 }, displayName + " Pan", juce::NormalisableRange<float> { -100.0f, 100.0f, 0.001f, 1.0f }, 0.0f));

            return result;
        }

        virtual vector<string> getAudioParametersIds() override
        {
            vector<string> ids;
            ids.push_back (uniqueId + BaseParameterIdSuffixes::level);
            ids.push_back (uniqueId + BaseParameterIdSuffixes::order);
            ids.push_back (uniqueId + BaseParameterIdSuffixes::pan);

            return ids;
        }

        static vector<string> getAudioParametersIdsStatic (int instanceNumber)
        {
            vector<string> ids;
            auto instanceUniqueId = getTypeIdStatic() + std::to_string (instanceNumber);
            ids.push_back (instanceUniqueId + BaseParameterIdSuffixes::level);
            ids.push_back (instanceUniqueId + BaseParameterIdSuffixes::order);
            ids.push_back (instanceUniqueId + BaseParameterIdSuffixes::pan);

            return ids;
        }

        virtual void parameterChanged (const juce::String& parameterID, float newValue) override
        {
            if (parameterID == (String) (uniqueId + BaseParameterIdSuffixes::level))
            {
                params.level = newValue;
            }
            else if (parameterID == (String) (uniqueId + BaseParameterIdSuffixes::order))
            {
                params.order = static_cast<int> (newValue);
            }
            else if (parameterID == (String) (uniqueId + BaseParameterIdSuffixes::pan))
            {
                params.pan = newValue;
            }

            for (auto& collector : harmonicCollectors)
            {
                collector->setParams (params);
            }
        }

        virtual vector<ModTargetData> getModTargetData() override
        {
            auto audioParams = getAudioParameters();
            vector<ModTargetData> modTargets;
            for (auto& param : audioParams->getParameters (false))
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
        vector<shared_ptr<HC>> harmonicCollectors;
        AdditiveSynthHarmonicCollectorParams params;
        int currentNote = -1;

        virtual string getDisplayName() const
        {
            return HC::getDisplayNameStatic() + " " + std::to_string (uniqueIdIndex + 1);
        }

        static string getTypeIdStatic()
        {
            if (typeId.empty())
            {
                juce::String className = typeid (HC).name();
                className = className.replace ("class", "")
                                .replace ("tmf::", "")
                                .replace ("HarmonicCollector", "")
                                .replace (" ", "");
                typeId = className.toStdString();
            }

            return typeId;
        }

    private:
        string uniqueId;
        int uniqueIdIndex = 0;
        inline static int uniqueIdCount = 0;
        inline static string typeId = "";
    };
}
