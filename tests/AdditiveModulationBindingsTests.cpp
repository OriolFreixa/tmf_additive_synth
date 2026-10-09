#include "tmf_additive_synth/tmf_additive_synth.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

TEST_CASE ("Cached collector bindings match the string modulation API", "[tmf_additive_synth][modulation-bindings]")
{
    const auto suffix = GENERATE ("_LEVEL", "_PAN", "_ORDER");
    const auto offset = GENERATE (-1.0f, -0.0001f, 0.0f, 0.0001f, 0.37f, 1.0f);
    tmf::HarmonicCollectorOctaveHarmonics bound, reference;
    bound.setId ("BIND");
    reference.setId ("BIND");
    bound.setParams ({ 0.5f, 0.0f, 3 });
    reference.setParams ({ 0.5f, 0.0f, 3 });
    const juce::String id = juce::String ("BIND") + suffix;
    const auto binding = bound.bindModulationTarget (id);
    REQUIRE (binding.has_value());
    binding->update (offset);
    REQUIRE (reference.updateModTargetValue (id, offset));
    REQUIRE (binding->position().has_value());
    CHECK (*binding->position() == Catch::Approx (*reference.getEffectiveModulationTargetPosition (id)));
    CHECK (bound.getOrder() == reference.getOrder());
    CHECK_FALSE (bound.bindModulationTarget ("BIND_UNKNOWN").has_value());
}

TEST_CASE ("Cached harmonic bindings retain numeric identity across parameter changes", "[tmf_additive_synth][modulation-bindings]")
{
    const auto index = GENERATE (0, tmf::maxBoundValue - 1);
    const auto pan = GENERATE (false, true);
    tmf::HarmonicCollectorOctaveHarmonics bound, reference;
    bound.setId ("BIND");
    reference.setId ("BIND");
    const juce::String id = "BIND" + (pan ? tmf::HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getPan (index) : tmf::HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getLevel (index));
    const auto binding = bound.bindModulationTarget (id);
    REQUIRE (binding.has_value());
    for (const float offset : { -1.0f, 0.0f, 0.37f, 1.0f })
    {
        binding->update (offset);
        REQUIRE (reference.updateModTargetValue (id, offset));
        CHECK (*binding->position() == Catch::Approx (*reference.getEffectiveModulationTargetPosition (id)));
    }
    tmf::HarmonicCollectorOctaveHarmonicsParams parameters;
    parameters.levels[static_cast<size_t> (index)] = 0.1f;
    parameters.pans[static_cast<size_t> (index)] = -90.0f;
    bound.setHarmonicParams (parameters);
    reference.setHarmonicParams (parameters);
    CHECK (*binding->position() == Catch::Approx (*reference.getEffectiveModulationTargetPosition (id)));
}

TEST_CASE ("Cached additive bindings survive collector reordering and settle without rebuilds", "[tmf_additive_synth][modulation-bindings]")
{
    auto collector = std::make_shared<tmf::HarmonicCollectorOctaveHarmonics>();
    collector->setId ("BIND");
    collector->setParams ({ 0.5f, 0.0f, 1 });
    tmf::VoiceInterceptorAdditiveSynth voice;
    voice.addHarmonicCollector (collector);
    auto earlierCollector = std::make_shared<tmf::HarmonicCollectorOctaveHarmonics>();
    earlierCollector->setId ("BIND_OTHER");
    earlierCollector->setParams ({ 0.5f, 0.0f, 0 });
    voice.addHarmonicCollector (earlierCollector);
    const auto binding = voice.bindModulationTarget ("BIND_LEVEL");
    const auto order = voice.bindModulationTarget ("BIND_ORDER");
    order.update (0.01f);
    voice.orderCollectors();
    binding.update (0.25f);
    CHECK (*binding.position() == Catch::Approx (0.75f));
    voice.prepareToPlay (48000, 128, 2);
    voice.startNote (60, 1.0f, 8192);
    juce::AudioBuffer<float> buffer (2, 128);
    for (int tick = 0; tick < 20; ++tick)
    {
        binding.update (0.25f);
        buffer.clear();
        voice.processBlock (buffer, 0, 128);
    }
    CHECK (*binding.position() == Catch::Approx (0.75f));
    CHECK_FALSE (voice.bindModulationTarget ("UNKNOWN").position().has_value());
}
