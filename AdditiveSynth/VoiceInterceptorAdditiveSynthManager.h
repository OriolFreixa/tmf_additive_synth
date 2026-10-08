/*
  ==============================================================================

    VoiceInterceptorAdditiveSynthManager.h
    Created: 21/4/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "AdditiveSynthHarmonicCollectorManager.h"
#include "VoiceInterceptorAdditiveSynth.h"
#include "tmf_intercept_synth/tmf_intercept_synth.h"
#include <map>

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
                addCollector (collector);
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

        vector<SynthParameterDescription> getParameterDescriptions() override
        {
            return parameterDescriptions;
        }

        void applyParameterSnapshot (std::span<const float> values) override
        {
            jassert (values.size() == parameterDescriptions.size());
            size_t offset = 0;
            for (size_t index = 0; index < harmonicCollectorManagers.size(); ++index)
            {
                const auto count = collectorParameterCounts[index];
                harmonicCollectorManagers[index]->applyParameterSnapshot (values.subspan (offset, count));
                offset += count;
            }
            for (const auto& interceptor : voiceInterceptors)
            {
                if (interceptor == nullptr)
                    continue;
                interceptor->markWavetableAsNeedsRefresh();
                interceptor->markCollectorsAsNeedOrder();
            }
        }

    private:
        void addCollector (shared_ptr<HarmonicCollectorManagerInterface> collector)
        {
            auto descriptions = collector->getParameterDescriptions();
            for (auto& description : descriptions)
                registerCollectorParameter (description, collector);

            harmonicCollectorManagers.push_back (collector);
            collectorParameterCounts.push_back (descriptions.size());
        }

        void registerCollectorParameter (SynthParameterDescription description,
            const shared_ptr<HarmonicCollectorManagerInterface>& collector)
        {
            auto inserted = parameterOwners.emplace (description.id, collector);
            jassert (inserted.second);
            if (!inserted.second)
                return;

            description.groupPath.insert (description.groupPath.begin(),
                SynthParameterGroupDescription { "ADDITIVESYNTH", "Additive Synth", "_" });
            parameterDescriptions.push_back (std::move (description));
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
        vector<SynthParameterDescription> parameterDescriptions;
        map<string, shared_ptr<HarmonicCollectorManagerInterface>> parameterOwners;
        vector<size_t> collectorParameterCounts;
    };
}
