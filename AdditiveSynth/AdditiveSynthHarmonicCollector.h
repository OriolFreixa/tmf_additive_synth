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
        AdditiveSynthHarmonicCollector (int tableSize)
        {        
            this->tableSize = tableSize;
        }
        virtual ~AdditiveSynthHarmonicCollector() = default;

        virtual void collectHarmonics(float* data) = 0;

    private:
        int tableSize;
    };

    class HarmonicCollectorManagerInterface
    {
    public:
        HarmonicCollectorManagerInterface() = default;
        virtual ~HarmonicCollectorManagerInterface() = default;
        virtual shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (int index) = 0;
    };

    template <class HC>
    class HarmonicCollectorManager : public HarmonicCollectorManagerInterface
    {
        static_assert (std::is_base_of<AdditiveSynthHarmonicCollector, HC>::value, "VI must inherit AdditiveSynthHarmonicCollector");

    public:
        HarmonicCollectorManager() = default;

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