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

            return std::move (result);
        };

        vector<string> getAudioParametersIds() override
        {
            // TODO: For each collector we need: Order, Power, Pan
            return {};
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