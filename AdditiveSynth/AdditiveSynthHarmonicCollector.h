/*
  ==============================================================================

    AdditiveSynthHarmonicCollector.h
    Created: 21/4/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once

namespace tmf
{
    namespace BaseParameterIdSuffixes
    {
        inline static const string level = "_LEVEL";
        inline static const string order = "_ORDER";
        inline static const string pan = "_PAN";
    }

    struct AdditiveSynthHarmonicCollectorParams
    {
        float level = 0.5;
        float pan = 0;
        int order = -1;
    };

    class AdditiveSynthHarmonicCollector
    {
    public:
        AdditiveSynthHarmonicCollector() = default;

        virtual ~AdditiveSynthHarmonicCollector() = default;

        void setId (string newId) { id = newId; }

        int getOrder() const { return order; }

        /// <summary>
        /// Sets the harmonics of the collector
        /// </summary>
        /// <param name="audioBuffer">Input/output audio buffer. Content in the frequency domain in packed format. Info: https://www.intel.com/content/www/us/en/docs/ipp/developer-guide-reference/2022-0/packed-formats.html</param>
        /// <param name="dataSize">Length of the audioBuffer</param>
        virtual void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, int dataSize) = 0;

        virtual bool waveTableRefreshNeeded() const { return level.isSmoothing() || pan.isSmoothing(); }

        void setParams (AdditiveSynthHarmonicCollectorParams params)
        {
            paramsValues = params;
            doUpdateParameters();
        }

        void prepareToPlay (double sampleRate, int numOutputChannels)
        {
            this->sampleRate = sampleRate;
            this->numChannels = numOutputChannels;
            level.reset(sampleRate, 0.01);
            pan.reset(sampleRate, 0.01);
        }

        void advanceSamples (int numSamples)
        {
            level.skip (numSamples);
            pan.skip (numSamples);
        }

        virtual void setCurrentNote (int note) { currentNote = note; }

        virtual void updateModTargetValue (juce::String paramid, float value)
        {
            jassert (id != "");
            if (paramid == (String) (id + BaseParameterIdSuffixes::level))
            {
                modValues.level = value;
            }
            else if (paramid == (String) (id + BaseParameterIdSuffixes::order))
            {
                modValues.order = juce::jmap(value, 0.f, 1000.f);
            }
            else if (paramid == (String) (id + BaseParameterIdSuffixes::pan))
            {
                // We want to cover the whole range, so 0 to 100 - (-100)
                modValues.pan = juce::jmap (value, 0.f, 200.f);
            }

            doUpdateParameters();
        }

    protected:
        void doUpdateParameters()
        {
            float limitedLevel = juce::jlimit (0.0f, 1.0f, modValues.level + paramsValues.level);
            float limitedPan = juce::jlimit (-100.0f, 100.0f, modValues.pan + paramsValues.pan);

            if (currentNote == -1)
            {
                this->level.setCurrentAndTargetValue (limitedLevel);
                this->pan.setCurrentAndTargetValue (limitedPan);
            }
            else
            {
                this->level.setTargetValue (limitedLevel);
                this->pan.setTargetValue (limitedPan);
            }

            float limitedOrder = juce::jlimit (-1, 1000, modValues.order + paramsValues.order);
            this->order = limitedOrder;
        }

        void applyPanAndGainAndRenderToBuffer(juce::AudioBuffer<float>& audioBlock, std::vector<float> data)
        {
            jassert (audioBlock.getNumChannels() == 2);

            int tableSize = data.size();
            juce::FloatVectorOperations::multiply (data.data(), level.getCurrentValue(), tableSize);

            // From [-100, 100] to [0, 1]
            auto normalisedPan = (pan.getCurrentValue() * 0.005) + 0.5;

            auto leftValue = (1.0 - normalisedPan) * 2;
            auto rightValue = normalisedPan * 2;

            auto auxVector = std::vector<float> (tableSize);
            juce::FloatVectorOperations::multiply (auxVector.data(), data.data(), leftValue, tableSize);
            juce::FloatVectorOperations::add (audioBlock.getWritePointer (0), auxVector.data(), tableSize);

            juce::FloatVectorOperations::multiply (auxVector.data(), data.data(), rightValue, tableSize);
            juce::FloatVectorOperations::add (audioBlock.getWritePointer (1), auxVector.data(), tableSize);
        }

        juce::SmoothedValue<float> level = 0.5;
        juce::SmoothedValue<float> pan = 0;
        int order = -1;

        AdditiveSynthHarmonicCollectorParams paramsValues;
        AdditiveSynthHarmonicCollectorParams modValues {0, 0, 0};

        float sampleRate = 0;
        int numChannels = 0;
        int currentNote = -1;

        string id = "";
    };

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

        int getOrder() { return params->order; }

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
            if (parameterID == (String)(id + BaseParameterIdSuffixes::level))
            {
                params.level = newValue;
            }
            else if (parameterID == (String)(id + BaseParameterIdSuffixes::order))
            {
                params.order = newValue;
            }
            else if (parameterID == (String) (id + BaseParameterIdSuffixes::pan))
            {
                params.pan = newValue;
            }

            for (auto& collector : harmonicCollectors)
            {
                collector->setParams(params);
            }
        }

        virtual vector<ModTargetData> getModTargetData()
        {
            auto params = getAudioParameters();
            vector<ModTargetData> modTargets;
            for (auto& param : params->getParameters(false))
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