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
#include "AdditiveSynthHarmonicCollectorManager.h"

namespace tmf
{
    using namespace std;
    class VoiceInterceptorAdditiveSynthManager : public VoiceInterceptorManager<VoiceInterceptorAdditiveSynth>,
                                                 public VoiceInterceptorManagerWithParameters,
                                                 public VoiceInterceptorWithModTargetsManager
    {
    public:
        VoiceInterceptorAdditiveSynthManager (vector<shared_ptr<HarmonicCollectorManagerInterface>> collectors)
        {
            for (auto collector : collectors)
            {
                addCollector (collector);
                auto collectorModTargetData = collector->getModTargetData();

                modTargetData.insert (modTargetData.end(), collectorModTargetData.begin(), collectorModTargetData.end());
            }
        }

        virtual shared_ptr<VoiceInterceptor> getOrCreateVoiceInterceptor (int index) override
        {
            jassert (index >= 0);
            if (index < 0)
                return {};

            const auto voiceIndex = static_cast<size_t> (index);
            if (voiceIndex >= voiceInterceptors.size())
            {
                voiceInterceptors.resize (voiceIndex + 1);

            }
            if (!voiceInterceptors[voiceIndex])
            {
                voiceInterceptors[voiceIndex] = make_shared<VoiceInterceptorAdditiveSynth>();
                addCollectorsToInterceptor (voiceInterceptors[voiceIndex], voiceIndex);

            }
            return dynamic_pointer_cast<VoiceInterceptor> (voiceInterceptors[voiceIndex]);
        }

        unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() override
        {
            // TODO: Add main parameters
            auto result = make_unique<juce::AudioProcessorParameterGroup> ("ADDITIVESYNTH", "ADDITIVESYNTH", "_");
            for (auto collectorManager : harmonicCollectorManagers)
            {
                auto params = collectorManager->getAudioParameters();
                result->addChild (std::move (params));
            }

            return result;
        }

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
                    apvts.addParameterListener (param, this);
                }
            }
        }

        void parameterChanged (const juce::String& parameterID, float) override
        {
            for (auto interceptor : voiceInterceptors)
            {
                interceptor->markWavetableAsNeedsRefresh();

                if (parameterID.contains (BaseParameterIdSuffixes::order))
                {
                    interceptor->markCollectorsAsNeedOrder();
                }
            }
        }

    private:

        void addCollector (shared_ptr<HarmonicCollectorManagerInterface> collector)
        {
            harmonicCollectorManagers.push_back (collector);
        }

        void addCollectorsToInterceptor (shared_ptr<VoiceInterceptorAdditiveSynth> interceptor, size_t index)
        {
            for (auto collectorManager : harmonicCollectorManagers)
            {
                auto hc = collectorManager->getOrCreateHarmonicCollector (index);
                interceptor->addHarmonicCollector (hc);
            }
        }
        vector<shared_ptr<HarmonicCollectorManagerInterface>> harmonicCollectorManagers;
    };
}
