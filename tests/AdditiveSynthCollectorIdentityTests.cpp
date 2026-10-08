#include "tmf_additive_synth/tmf_additive_synth.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    std::vector<juce::String> collectorParameterIds (tmf::HarmonicCollectorManagerInterface& collectorManager)
    {
        std::vector<juce::String> parameterIds;
        for (const auto& description : collectorManager.getParameterDescriptions())
            parameterIds.emplace_back (description.id);
        return parameterIds;
    }

    void checkRejectedCollectorIdentity (tmf::AdditiveSynthHarmonicCollector& collector,
        const std::vector<juce::String>& parameterIds,
        const juce::String& foreignId)
    {
        for (const auto& parameterId : parameterIds)
        {
            const auto baseline = collector.getEffectiveModulationTargetPosition (parameterId);
            REQUIRE (baseline.has_value());
            REQUIRE (collector.updateModTargetValue (parameterId, 0.25f));
            REQUIRE (collector.updateModTargetValue (parameterId, 0.0f));
            CHECK (collector.getEffectiveModulationTargetPosition (parameterId) == baseline);
            const auto suffix = parameterId.substring (static_cast<int> (collector.getId().size()));
            for (const auto& rejectedId : { foreignId + suffix, juce::String (collector.getId()) + "_extra" + suffix })
            {
                INFO (rejectedId.toStdString());
                CHECK_FALSE (collector.updateModTargetValue (rejectedId, 0.25f));
                CHECK_FALSE (collector.getEffectiveModulationTargetPosition (rejectedId).has_value());
                CHECK (collector.getEffectiveModulationTargetPosition (parameterId) == baseline);
            }
        }
        CHECK_FALSE (collector.updateModTargetValue (collector.getId() + "_UNKNOWN", 0.25f));
        CHECK_FALSE (collector.getEffectiveModulationTargetPosition (collector.getId() + "_UNKNOWN").has_value());
    }

    void configureIdentityTestCollector (const std::shared_ptr<tmf::AdditiveSynthHarmonicCollector>& collector)
    {
        collector->setParams ({ 0.25f, 0.0f, 5 });
        auto harmonics = std::dynamic_pointer_cast<tmf::HarmonicCollectorOctaveHarmonics> (collector);
        REQUIRE (harmonics != nullptr);
        tmf::HarmonicCollectorOctaveHarmonicsParams harmonicParams;
        harmonicParams.levels.fill (0.25f);
        harmonics->setHarmonicParams (harmonicParams);
    }

    void checkVoiceCollectorIdentity (int startIndex, bool reverseInsertion, const juce::String& parameterId)
    {
        tmf::HarmonicCollectorOctaveHarmonicsManager shortManager { 1 }, targetManager { startIndex };
        const auto shortCollector = shortManager.getOrCreateHarmonicCollector (0);
        const auto targetCollector = targetManager.getOrCreateHarmonicCollector (0);
        const auto referenceCollector = targetManager.getOrCreateHarmonicCollector (1);
        for (const auto& collector : { shortCollector, targetCollector, referenceCollector })
            configureIdentityTestCollector (collector);

        tmf::VoiceInterceptorAdditiveSynth voice;
        voice.addHarmonicCollector (reverseInsertion ? targetCollector : shortCollector);
        voice.addHarmonicCollector (reverseInsertion ? shortCollector : targetCollector);
        const auto shortId = juce::String (shortCollector->getId()) + parameterId.substring (static_cast<int> (targetCollector->getId().size()));
        const auto baseline = shortCollector->getEffectiveModulationTargetPosition (shortId);
        REQUIRE (baseline.has_value());
        for (const float offset : { 0.2f, 0.1f, 0.0f })
        {
            voice.updateModTargetValue (parameterId, offset);
            REQUIRE (referenceCollector->updateModTargetValue (parameterId, offset));
            const auto expected = referenceCollector->getEffectiveModulationTargetPosition (parameterId);
            REQUIRE (expected.has_value());
            CHECK (targetCollector->getEffectiveModulationTargetPosition (parameterId) == expected);
            CHECK (voice.getEffectiveModulationTargetPosition (parameterId) == expected);
            CHECK (shortCollector->getEffectiveModulationTargetPosition (shortId) == baseline);
        }
    }
}

TEST_CASE ("Base collector rejects foreign and malformed parameter identities", "[tmf_additive_synth][collector-identity]")
{
    tmf::HarmonicCollectorManager<tmf::HarmonicCollectorSine> collectorManager;
    const auto collector = collectorManager.getOrCreateHarmonicCollector (0);
    checkRejectedCollectorIdentity (*collector, collectorParameterIds (collectorManager), collector->getId() + "0");
}

TEST_CASE ("Octave collector rejects foreign and malformed parameter identities", "[tmf_additive_synth][collector-identity]")
{
    tmf::HarmonicCollectorOctavesManager collectorManager;
    const auto collector = collectorManager.getOrCreateHarmonicCollector (0);
    checkRejectedCollectorIdentity (*collector, collectorParameterIds (collectorManager), collector->getId() + "0");
}

TEST_CASE ("Octave harmonics collector rejects colliding colour identities", "[tmf_additive_synth][collector-identity]")
{
    tmf::HarmonicCollectorOctaveHarmonicsManager collectorManager { 1 };
    const auto collector = collectorManager.getOrCreateHarmonicCollector (0);
    configureIdentityTestCollector (collector);
    for (int startIndex = 10; startIndex <= 15; ++startIndex)
    {
        tmf::HarmonicCollectorOctaveHarmonicsManager foreignManager { startIndex };
        checkRejectedCollectorIdentity (*collector, collectorParameterIds (collectorManager), foreignManager.getId());
    }
}

TEST_CASE ("Additive voice isolates initial cached and reset colour modulation", "[tmf_additive_synth][collector-identity]")
{
    for (int startIndex = 10; startIndex <= 15; ++startIndex)
    {
        tmf::HarmonicCollectorOctaveHarmonicsManager targetManager { startIndex };
        for (const auto& parameterId : collectorParameterIds (targetManager))
        {
            INFO (parameterId.toStdString());
            checkVoiceCollectorIdentity (startIndex, false, parameterId);
            checkVoiceCollectorIdentity (startIndex, true, parameterId);
        }
    }
}
