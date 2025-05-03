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

            if (dynamic_cast<HarmonicCollectorManagerWithCustomParameters*> (collector.get()))
            {
                harmonicCollectorManagersWithParameters.push_back (dynamic_pointer_cast<HarmonicCollectorManagerWithCustomParameters> (collector));
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
            for (auto collectorManager : harmonicCollectorManagers)
            {
                auto params = collectorManager->getAudioParameters();
                result->addChild (std::move (params));
            }

            return std::move (result);
        };

        vector<string> getAudioParametersIds() override
        {
            vector<string> ids;
            for (auto collectorManager : harmonicCollectorManagers)
            {
                auto idsToAdd = collectorManager->getAudioParametersIds();
                ids.insert (ids.end(), idsToAdd.begin(), idsToAdd.end());
            }
            
            return ids;
        }

        virtual void setupAPVTS (juce::AudioProcessorValueTreeState& apvts) override
        {
            for (auto collectorManager : harmonicCollectorManagers)
            {
                auto idsToAdd = collectorManager->getAudioParametersIds();
                for (auto param : idsToAdd)
                {
                    apvts.addParameterListener (param, collectorManager.get());
                }
            }
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
        vector<shared_ptr<HarmonicCollectorManagerWithCustomParameters>> harmonicCollectorManagersWithParameters;
    };
}