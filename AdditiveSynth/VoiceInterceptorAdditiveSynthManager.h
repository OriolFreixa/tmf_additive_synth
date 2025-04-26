/*
  ==============================================================================

    VoiceInterceptorAdditiveSynthManager.h
    Created: 21/4/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "tmf_intercept_synth/tmf_intercept_synth.h"
#include "VoiceInterceptorAdditiveSynth.h"
#include "AdditiveSynthHarmonicCollector.h"

namespace tmf
{
    using namespace std;
    class VoiceInterceptorAdditiveSynthManager : public VoiceInterceptorManager<VoiceInterceptorAdditiveSynth>,
                                                 public VoiceInterceptorManagerWithParameters
    {
    public:
        VoiceInterceptorAdditiveSynthManager (vector<shared_ptr<HarmonicCollectorManagerInterface>> collectors)
        {
            for (auto collector : collectors)
            {
                addCollector (collector);
            }
        }

        void addCollector (shared_ptr<HarmonicCollectorManagerInterface> collector)
        {
            harmonicCollectorManagers.push_back (collector);

            if (dynamic_cast<HarmonicCollectorManagerWithParameters*> (collector.get()))
            {
                harmonicCollectorManagersWithParameters.push_back (dynamic_pointer_cast<HarmonicCollectorManagerWithParameters> (collector));
            }
        }

        virtual shared_ptr<VoiceInterceptor> getOrCreateVoiceInterceptor (int index) override
        {
            if (index >= voiceInterceptors.size())
            {
                voiceInterceptors.resize (index + 1);

            }
            if (!voiceInterceptors[index])
            {
                voiceInterceptors[index] = make_shared<VoiceInterceptorAdditiveSynth>();
                addCollectorsToInterceptor (voiceInterceptors[index], index);
            }
            return dynamic_pointer_cast<VoiceInterceptor> (voiceInterceptors[index]);
        }

        unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() override
        {
            // TODO
            auto result = make_unique<juce::AudioProcessorParameterGroup> ("ADDITIVESYNTH", "ADDITIVESYNTH", "_");
            int numOfCollectorManagers = harmonicCollectorManagers.size();
            for (auto collectorManager : harmonicCollectorManagers)
            {
                auto id = collectorManager->getId();
                result->addChild (make_unique<juce::AudioParameterFloat> (juce::ParameterID { id + "_LEVEL", 1 }, id + "Level", juce::NormalisableRange<float> { 0, 1, 0.001, 0.8 }, 0.5));
                result->addChild (make_unique<juce::AudioParameterInt> (juce::ParameterID { id + "_ORDER", 1 }, id + "Order", -1, numOfCollectorManagers, -1));
                result->addChild (make_unique<juce::AudioParameterFloat> (juce::ParameterID { id + "_PAN", 1 }, id + "Pan", juce::NormalisableRange<float> { -100, 100, 0.001, 1 }, 0));
            }
            return std::move (result);
        };

        vector<string> getAudioParametersIds() override
        {
            vector<string> ids;
            for (auto collectorManager : harmonicCollectorManagers)
            {
                auto id = collectorManager->getId();
                ids.push_back (id + "_LEVEL");
                ids.push_back (id + "_ORDER");
                ids.push_back (id + "_PAN");
            }
            
            return ids;
        }

        virtual void setupAPVTS (juce::AudioProcessorValueTreeState& apvts) override
        {
            auto params = getAudioParametersIds();
            for (auto param : params)
            {
                apvts.addParameterListener (param, this);
            }

            // TODO: Collect the custom parameters for each collector
        }

        void parameterChanged (const juce::String& parameterID, float newValue) override
        {
        }

    private:
        void addCollectorsToInterceptor (shared_ptr<VoiceInterceptorAdditiveSynth> interceptor, int index)
        {
            for (auto collectorManager : harmonicCollectorManagers)
            {
                auto hc = collectorManager->getOrCreateHarmonicCollector (index);
                interceptor->addHarmonicCollector (hc);
            }
        }
        vector<shared_ptr<HarmonicCollectorManagerInterface>> harmonicCollectorManagers;
        vector<shared_ptr<HarmonicCollectorManagerWithParameters>> harmonicCollectorManagersWithParameters;
    };
}