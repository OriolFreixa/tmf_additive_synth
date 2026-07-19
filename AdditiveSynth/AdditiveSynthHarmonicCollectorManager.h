/*
  ==============================================================================

    AdditiveSynthHarmonicCollectorManager.h
    Created: 24/6/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/
#pragma once
#include "AdditiveSynthHarmonicCollector.h"
#include "tmf_intercept_synth/synth/SynthParameterDescription.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <string>
#include <vector>

namespace tmf
{
    using namespace juce;
    using namespace std;

    class HarmonicCollectorManagerInterface : public juce::AudioProcessorValueTreeState::Listener
    {
    public:
        HarmonicCollectorManagerInterface() = default;
        virtual ~HarmonicCollectorManagerInterface() = default;
        virtual std::shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (size_t index) = 0;
        virtual std::string getId() const = 0;
        virtual std::vector<SynthParameterDescription> getParameterDescriptions() = 0;
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

        virtual vector<SynthParameterDescription> getParameterDescriptions() override
        {
            auto id = getId();
            auto displayName = getDisplayName();
            SynthParameterGroupPath groupPath { { id, displayName, "_" } };
            return {
                makeFloatSynthParameter (groupPath, id + BaseParameterIdSuffixes::level, displayName + " Level", { 0.0f, 1.0f, 0.001f, 0.65f }, 0.5f, true),
                makeIntSynthParameter (groupPath, id + BaseParameterIdSuffixes::order, displayName + " Order", -1, 1000, -1, true),
                makeFloatSynthParameter (std::move (groupPath), id + BaseParameterIdSuffixes::pan, displayName + " Pan", { -100.0f, 100.0f, 0.001f, 1.0f }, 0.0f, true)
            };
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
