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
    class AdditiveSynthHarmonicCollector
    {
    public:
        AdditiveSynthHarmonicCollector () = default;
        virtual ~AdditiveSynthHarmonicCollector() = default;

        virtual void collectHarmonics(float* data, int dataSize) = 0;
    };

    class HarmonicCollectorManagerInterface
    {
    public:
        HarmonicCollectorManagerInterface() = default;
        virtual ~HarmonicCollectorManagerInterface() = default;
        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (int index) = 0;
        virtual string getId() const { return typeid(*this).name(); }
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
            juce::String className = typeid (HC).name(); 
            className = className.replace ("class", "")
                            .replace ("tmf::", "")
                            .replace ("HarmonicCollector", "")
                            .replace (" ", "");
            return className.toStdString();
        }

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

    protected:
        vector<shared_ptr<HC>> harmonicCollectors;
    };

    class HarmonicCollectorManagerWithParameters : public juce::AudioProcessorValueTreeState::Listener
    {
    public:
        HarmonicCollectorManagerWithParameters() = default;
        virtual ~HarmonicCollectorManagerWithParameters() = default;
        virtual unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() = 0;
        virtual vector<string> getAudioParametersIds() = 0;
        virtual void parameterChanged (const juce::String& parameterID, float newValue) = 0;
    };
}