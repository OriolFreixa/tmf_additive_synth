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

    class AdditiveSynthHarmonicCollector
    {
    public:
        AdditiveSynthHarmonicCollector () = default;
        virtual ~AdditiveSynthHarmonicCollector() = default;

        virtual void collectHarmonics(float* data, int dataSize) = 0;
    };

    class HarmonicCollectorManagerInterface : public juce::AudioProcessorValueTreeState::Listener
    {
    public:
        HarmonicCollectorManagerInterface() = default;
        virtual ~HarmonicCollectorManagerInterface() = default;
        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (int index) = 0;
        virtual string getId() const { return typeid(*this).name(); }
        virtual unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() = 0;
        virtual vector<string> getAudioParametersIds() = 0;
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

        int getOrder() { return order; }

        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (int index) override
        {
            if (index >= harmonicCollectors.size())
            {
                harmonicCollectors.resize (index + 1);
            }
            if (!harmonicCollectors[index])
            {
                harmonicCollectors[index] = make_shared<HC>();
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
            vector<string> ids;
            auto id = getId();
            ids.push_back (id + "_LEVEL");
            ids.push_back (id + "_ORDER");
            ids.push_back (id + "_PAN");

            return ids;
        }

        virtual void parameterChanged (const juce::String& parameterID, float newValue) 
        {
            if (parameterID == (String)(id + BaseParameterIdSuffixes::level))
            {
                level = newValue;
            }
            else if (parameterID == (String)(id + BaseParameterIdSuffixes::order))
            {
                order = newValue;
            }
            else if (parameterID == (String) (id + BaseParameterIdSuffixes::pan))
            {
                pan = newValue;
            }
        }

    protected:
        inline static string id = "";
        vector<shared_ptr<HC>> harmonicCollectors;
        float level = 0, pan = 0;
        int order = -1;
    };

    class HarmonicCollectorManagerWithCustomParameters
    {
    public:
        HarmonicCollectorManagerWithCustomParameters() = default;
        virtual ~HarmonicCollectorManagerWithCustomParameters() = default;
    };
}