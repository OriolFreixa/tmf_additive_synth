#include "tmf_additive_synth/tmf_additive_synth.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace
{
    using Overrides = std::map<std::string, float>;

    std::shared_ptr<tmf::HarmonicCollectorWithControlsManager> makeManager (const tmf::ControlledCollectorDefinition& definition)
    {
        return std::make_shared<tmf::HarmonicCollectorWithControlsManager> (definition);
    }

    /// Reads full level, visible order and centred pan, then the control defaults, unless overridden.
    tmf::AdditiveBaseWave::ParameterReader makeReader (
        const tmf::ControlledCollectorDefinition& definition,
        Overrides overrides)
    {
        return [&definition, overrides = std::move (overrides)] (const std::string& parameterId) {
            if (const auto found = overrides.find (parameterId); found != overrides.end())
                return found->second;

            if (parameterId == definition.levelId())
                return 1.0f;

            if (parameterId == definition.orderId())
                return 0.0f;

            if (parameterId == definition.panId())
                return 0.0f;

            for (size_t index = 0; index < definition.controls.size(); ++index)
                if (parameterId == definition.controlId (index))
                    return definition.controls[index].defaultValue;

            for (size_t index = 0; index < tmf::harmonicStereo::controlCount; ++index)
                if (parameterId == definition.stereoControlId (index))
                    return tmf::harmonicStereoControlSpecs()[index].defaultValue;

            FAIL ("Unexpected parameter " << parameterId);
            return 0.0f;
        };
    }

    juce::AudioBuffer<float> renderCycle (const tmf::ControlledCollectorDefinition& definition, Overrides overrides = {})
    {
        tmf::AdditiveBaseWave wave { { makeManager (definition) } };
        return wave.render (makeReader (definition, std::move (overrides)));
    }

    float channelMagnitude (const juce::AudioBuffer<float>& cycle, int channel)
    {
        return cycle.getMagnitude (channel, 0, cycle.getNumSamples());
    }

    float sampleAt (const juce::AudioBuffer<float>& cycle, float phase)
    {
        return cycle.getSample (0, static_cast<int> (phase * static_cast<float> (waveTableSize)));
    }

    std::vector<float> renderSpectrum (const tmf::ControlledCollectorDefinition& definition, const std::vector<float>& controls)
    {
        tmf::HarmonicCollectorWithControls collector { definition };
        collector.prepareToPlay (44100.0, 2);
        collector.setParams ({ 1.0f, 0.0f, 0 });
        collector.setControlValues (controls);
        juce::AudioBuffer<float> table { 2, waveTableSize };
        table.clear();
        collector.collectHarmonics (table, static_cast<size_t> (waveTableSize));
        return { table.getReadPointer (0), table.getReadPointer (0) + waveTableSize };
    }

    float imaginaryOf (const std::vector<float>& spectrum, size_t harmonic) { return spectrum[2u * harmonic + 1u]; }

    float realOf (const std::vector<float>& spectrum, size_t harmonic) { return spectrum[2u * harmonic]; }

    std::vector<float> uncappedParityControls()
    {
        return { 1.0f, 0.0f, 512.0f, 0.0f };
    }

    std::vector<float> parityValues (float level, float lowH, float lowSlope, float highH, float highSlope)
    {
        return { level, 0.0f, 0.0f, lowH, lowSlope, highH, highSlope };
    }

    void configureStackedParityVoice (tmf::VoiceInterceptorAdditiveSynth& voice, size_t collectorCount)
    {
        auto manager = makeManager (tmf::unevenCollectorDefinition());
        manager->applyParameterSnapshot (parityValues (1.0f, 1.0f, 0.0f, 512.0f, 0.0f));
        for (size_t index = 0; index < collectorCount; ++index)
            voice.addHarmonicCollector (manager->getOrCreateHarmonicCollector (index));

        voice.prepareToPlay (44100.0, 128, 2);
        voice.startNote (60, 1.0f, 8192);
    }

    juce::AudioBuffer<float> renderVoice (size_t collectorCount)
    {
        tmf::VoiceInterceptorAdditiveSynth voice;
        configureStackedParityVoice (voice, collectorCount);
        juce::AudioBuffer<float> buffer { 2, 512 };
        buffer.clear();
        voice.processBlock (buffer, 0, buffer.getNumSamples());
        return buffer;
    }
}

TEST_CASE ("Harmonic volume multipliers weight real and imaginary entries alike", "[tmf_additive_synth][wavetable]")
{
    const auto multipliers = tmf::additiveWavetable::createHarmonicVolumeMultipliers();

    CHECK (multipliers[0] == 1.0f);
    CHECK (multipliers[1] == 1.0f);
    CHECK (multipliers[6] == Catch::Approx (1.0f / 3.0f));
    CHECK (multipliers[7] == Catch::Approx (1.0f / 3.0f));
    CHECK (multipliers[waveTableSize - 1] == Catch::Approx (1.0f / 1023.0f));
}

TEST_CASE ("Saturation keeps overlapping harmonics at one and preserves panning", "[tmf_additive_synth][wavetable]")
{
    juce::AudioBuffer<float> table { 2, 16 };
    table.clear();
    table.setSample (0, 4, 3.0f);
    table.setSample (1, 4, 1.0f);
    table.setSample (0, 6, 0.4f);
    table.setSample (1, 6, 0.4f);
    table.setSample (0, 8, 4.0f);
    table.setSample (1, 8, 0.0f);

    tmf::additiveWavetable::saturateStereoHarmonics (table);

    CHECK (table.getSample (0, 4) == Catch::Approx (1.5f));
    CHECK (table.getSample (1, 4) == Catch::Approx (0.5f));
    CHECK (table.getSample (0, 6) == Catch::Approx (0.4f));
    CHECK (table.getSample (0, 8) == Catch::Approx (2.0f));
    CHECK (table.getSample (1, 8) == Catch::Approx (0.0f));
}

TEST_CASE ("Saturation limits the mid magnitude across real and imaginary parts", "[tmf_additive_synth][wavetable]")
{
    juce::AudioBuffer<float> table { 2, 16 };
    table.clear();
    for (const auto channel : { 0, 1 })
    {
        table.setSample (channel, 4, 0.9f);
        table.setSample (channel, 5, 1.2f);
    }

    tmf::additiveWavetable::saturateStereoHarmonics (table);

    const auto magnitude = std::hypot (table.getSample (0, 4), table.getSample (0, 5));
    CHECK (magnitude == Catch::Approx (1.0f));
    CHECK (table.getSample (0, 4) / table.getSample (0, 5) == Catch::Approx (0.75f));
}

TEST_CASE ("Triangle collector exposes its controls under the Shapes family", "[tmf_additive_synth][shapes]")
{
    auto manager = makeManager (tmf::triangleCollectorDefinition());
    const auto descriptions = manager->getParameterDescriptions();

    REQUIRE (descriptions.size() == 9);
    CHECK (descriptions[0].id == "ShapeTriangle_LEVEL");
    CHECK (descriptions[1].id == "ShapeTriangle_ORDER");
    CHECK (descriptions[2].id == "ShapeTriangle_PAN");
    CHECK (descriptions[3].id == "ShapeTriangle_FLATTEN");
    CHECK (descriptions[4].id == "ShapeTriangle_BASE");
    CHECK (descriptions[5].id == "ShapeTriangle_CURVE");
    CHECK (descriptions[3].name == "Triangle Flatten");
    CHECK (descriptions[3].getDisplayGroupPath() == juce::StringArray { "Shapes", "Triangle" });
    CHECK (descriptions[1].defaultValue == -1.0f);
    CHECK (descriptions[5].canBeModulationTarget);
}

TEST_CASE ("Every shape and parity definition lists unique parameter ids", "[tmf_additive_synth][shapes]")
{
    std::vector<std::string> allIds;
    for (const auto* definition : { &tmf::triangleCollectorDefinition(),
             &tmf::sawCollectorDefinition(),
             &tmf::squareCollectorDefinition(),
             &tmf::unevenCollectorDefinition(),
             &tmf::evenCollectorDefinition() })
    {
        const auto descriptions = makeManager (*definition)->getParameterDescriptions();
        const auto expectedIds = definition->allParameterIds();
        REQUIRE (descriptions.size() == expectedIds.size());
        for (size_t index = 0; index < descriptions.size(); ++index)
            CHECK (descriptions[index].id == expectedIds[index]);

        allIds.insert (allIds.end(), expectedIds.begin(), expectedIds.end());
    }

    std::sort (allIds.begin(), allIds.end());
    CHECK (std::adjacent_find (allIds.begin(), allIds.end()) == allIds.end());
}

TEST_CASE ("Triangle default is a unit sine-phase triangle", "[tmf_additive_synth][shapes]")
{
    const auto cycle = renderCycle (tmf::triangleCollectorDefinition());

    CHECK (sampleAt (cycle, 0.0f) == Catch::Approx (0.0f).margin (0.02));
    CHECK (sampleAt (cycle, 0.125f) == Catch::Approx (0.5f).margin (0.02));
    CHECK (sampleAt (cycle, 0.25f) == Catch::Approx (1.0f).margin (0.02));
    CHECK (sampleAt (cycle, 0.75f) == Catch::Approx (-1.0f).margin (0.02));
    CHECK (cycle.getSample (0, 300) == Catch::Approx (cycle.getSample (1, 300)));
}

TEST_CASE ("Triangle flatten clips the tip into a plateau", "[tmf_additive_synth][shapes]")
{
    const auto flattened = renderCycle (tmf::triangleCollectorDefinition(), { { "ShapeTriangle_FLATTEN", 0.5f } });

    CHECK (sampleAt (flattened, 0.2f) == Catch::Approx (1.0f).margin (0.03));
    CHECK (sampleAt (flattened, 0.25f) == Catch::Approx (1.0f).margin (0.03));
    CHECK (sampleAt (flattened, 0.3f) == Catch::Approx (1.0f).margin (0.03));
    CHECK (sampleAt (flattened, 0.05f) == Catch::Approx (0.2f / 0.525f).margin (0.03));
}

TEST_CASE ("Triangle base pulls the base together", "[tmf_additive_synth][shapes]")
{
    const auto wide = renderCycle (tmf::triangleCollectorDefinition());
    const auto narrow = renderCycle (tmf::triangleCollectorDefinition(), { { "ShapeTriangle_BASE", 1.0f } });

    CHECK (sampleAt (wide, 0.1f) == Catch::Approx (0.4f).margin (0.02));
    CHECK (std::abs (sampleAt (narrow, 0.1f)) < 0.05f);
    CHECK (sampleAt (narrow, 0.25f) == Catch::Approx (1.0f).margin (0.05));
}

TEST_CASE ("Triangle curve bends both lines in opposite directions", "[tmf_additive_synth][shapes]")
{
    const auto sagging = renderCycle (tmf::triangleCollectorDefinition(), { { "ShapeTriangle_CURVE", 1.0f } });
    const auto straight = renderCycle (tmf::triangleCollectorDefinition());
    const auto bulging = renderCycle (tmf::triangleCollectorDefinition(), { { "ShapeTriangle_CURVE", -1.0f } });

    CHECK (sampleAt (sagging, 0.125f) == Catch::Approx (0.25f).margin (0.03));
    CHECK (sampleAt (straight, 0.125f) == Catch::Approx (0.5f).margin (0.03));
    CHECK (sampleAt (bulging, 0.125f) == Catch::Approx (0.707f).margin (0.03));
    CHECK (sampleAt (sagging, 0.625f) == Catch::Approx (-sampleAt (sagging, 0.125f)).margin (0.03));
}

TEST_CASE ("Saw default falls from positive to negative", "[tmf_additive_synth][shapes]")
{
    const auto cycle = renderCycle (tmf::sawCollectorDefinition());

    CHECK (sampleAt (cycle, 0.25f) == Catch::Approx (0.5f).margin (0.03));
    CHECK (sampleAt (cycle, 0.5f) == Catch::Approx (0.0f).margin (0.03));
    CHECK (sampleAt (cycle, 0.75f) == Catch::Approx (-0.5f).margin (0.03));
}

TEST_CASE ("Saw flatten and curve reshape the ramp", "[tmf_additive_synth][shapes]")
{
    const auto flattened = renderCycle (tmf::sawCollectorDefinition(), { { "ShapeSaw_FLATTEN", 0.5f } });
    const auto sagging = renderCycle (tmf::sawCollectorDefinition(), { { "ShapeSaw_CURVE", 1.0f } });

    CHECK (sampleAt (flattened, 0.1f) == Catch::Approx (1.0f).margin (0.04));
    CHECK (sampleAt (flattened, 0.9f) == Catch::Approx (-1.0f).margin (0.04));
    CHECK (sampleAt (sagging, 0.125f) == Catch::Approx (0.5625f).margin (0.03));
}

TEST_CASE ("Square at half width has only odd harmonics", "[tmf_additive_synth][shapes]")
{
    const auto spectrum = renderSpectrum (tmf::squareCollectorDefinition(), { 0.5f, 0.0f });

    CHECK (imaginaryOf (spectrum, 1) == Catch::Approx (-2.0f / juce::MathConstants<float>::pi).margin (1e-4));
    CHECK (std::abs (imaginaryOf (spectrum, 2)) < 1e-4f);
    CHECK (imaginaryOf (spectrum, 3) == Catch::Approx (imaginaryOf (spectrum, 1)).margin (1e-4));
    CHECK (std::abs (realOf (spectrum, 1)) < 1e-4f);
}

TEST_CASE ("Square width adds even harmonics and moves the pulse", "[tmf_additive_synth][shapes]")
{
    const auto narrowSpectrum = renderSpectrum (tmf::squareCollectorDefinition(), { 0.25f, 0.0f });
    const auto cycle = renderCycle (tmf::squareCollectorDefinition(), { { "ShapeSquare_WIDTH", 0.25f } });

    CHECK (std::abs (imaginaryOf (narrowSpectrum, 2)) > 0.1f);
    // The spectrum carries no DC, so a pulse that is not centred is shifted to zero mean: 2 * (1 - width) and -2 * width.
    CHECK (sampleAt (cycle, 0.1f) == Catch::Approx (1.5f).margin (0.03));
    CHECK (sampleAt (cycle, 0.6f) == Catch::Approx (-0.5f).margin (0.03));
}

TEST_CASE ("Square edge softens the high harmonics", "[tmf_additive_synth][shapes]")
{
    const auto sharp = renderSpectrum (tmf::squareCollectorDefinition(), { 0.5f, 0.0f });
    const auto soft = renderSpectrum (tmf::squareCollectorDefinition(), { 0.5f, 0.5f });

    CHECK (std::abs (imaginaryOf (soft, 1)) > 0.85f * std::abs (imaginaryOf (sharp, 1)));
    CHECK (std::abs (imaginaryOf (soft, 41)) < 0.2f * std::abs (imaginaryOf (sharp, 41)));
}

TEST_CASE ("Uneven collector writes only odd harmonics into the real entries", "[tmf_additive_synth][parity]")
{
    const auto spectrum = renderSpectrum (tmf::unevenCollectorDefinition(), uncappedParityControls());

    CHECK (realOf (spectrum, 1) == Catch::Approx (1.0f));
    CHECK (realOf (spectrum, 2) == 0.0f);
    CHECK (realOf (spectrum, 3) == Catch::Approx (1.0f));
    CHECK (realOf (spectrum, 100) == 0.0f);
    CHECK (imaginaryOf (spectrum, 3) == 0.0f);
}

TEST_CASE ("Even collector writes only even harmonics", "[tmf_additive_synth][parity]")
{
    const auto spectrum = renderSpectrum (tmf::evenCollectorDefinition(), uncappedParityControls());

    CHECK (realOf (spectrum, 1) == 0.0f);
    CHECK (realOf (spectrum, 2) == Catch::Approx (1.0f));
    CHECK (realOf (spectrum, 3) == 0.0f);
    CHECK (realOf (spectrum, 100) == Catch::Approx (1.0f));
}

TEST_CASE ("Parity high cutoff and slope shape the upper harmonics", "[tmf_additive_synth][parity]")
{
    const auto gentle = renderSpectrum (tmf::unevenCollectorDefinition(), { 1.0f, 0.0f, 9.0f, 6.0206f });
    const auto steep = renderSpectrum (tmf::unevenCollectorDefinition(), { 1.0f, 0.0f, 9.0f, 24.0f });

    CHECK (realOf (gentle, 9) == Catch::Approx (1.0f));
    CHECK (realOf (gentle, 17) == Catch::Approx (9.0f / 17.0f).margin (1e-3));
    CHECK (realOf (steep, 17) < realOf (gentle, 17));
    CHECK (realOf (steep, 33) < realOf (steep, 17));
}

TEST_CASE ("Parity low cutoff and slope lift the harmonics approaching it", "[tmf_additive_synth][parity]")
{
    const auto gentle = renderSpectrum (tmf::unevenCollectorDefinition(), { 16.0f, 6.0206f, 512.0f, 0.0f });
    const auto steep = renderSpectrum (tmf::unevenCollectorDefinition(), { 16.0f, 24.0f, 512.0f, 0.0f });

    CHECK (realOf (gentle, 17) == Catch::Approx (1.0f));
    CHECK (realOf (gentle, 9) == Catch::Approx (9.0f / 16.0f).margin (1e-3));
    CHECK (realOf (steep, 9) < realOf (gentle, 9));
    CHECK (realOf (steep, 3) < realOf (steep, 9));
}

TEST_CASE ("Parity slope of zero leaves the spectrum flat", "[tmf_additive_synth][parity]")
{
    const auto spectrum = renderSpectrum (tmf::evenCollectorDefinition(), { 64.0f, 0.0f, 8.0f, 0.0f });

    CHECK (realOf (spectrum, 2) == Catch::Approx (1.0f));
    CHECK (realOf (spectrum, 200) == Catch::Approx (1.0f));
}

TEST_CASE ("Parity harmonic weights follow the documented slope law", "[tmf_additive_synth][parity]")
{
    using tmf::paritySpectrum::harmonicWeight;

    CHECK (harmonicWeight (6.0f, 4.0f, 6.0206f, 8.0f, 6.0206f) == Catch::Approx (1.0f));
    CHECK (harmonicWeight (16.0f, 4.0f, 6.0206f, 8.0f, 6.0206f) == Catch::Approx (0.5f).margin (1e-3));
    CHECK (harmonicWeight (2.0f, 4.0f, 6.0206f, 8.0f, 6.0206f) == Catch::Approx (0.5f).margin (1e-3));
    CHECK (tmf::paritySpectrum::belongsTo (3, tmf::paritySpectrum::Parity::uneven));
    CHECK_FALSE (tmf::paritySpectrum::belongsTo (3, tmf::paritySpectrum::Parity::even));
}

TEST_CASE ("Collector with controls caches its spectrum until a control moves", "[tmf_additive_synth][shapes]")
{
    tmf::HarmonicCollectorWithControls collector { tmf::squareCollectorDefinition() };
    collector.prepareToPlay (44100.0, 2);
    collector.setParams ({ 1.0f, 0.0f, 0 });

    const auto render = [&collector] {
        juce::AudioBuffer<float> table { 2, waveTableSize };
        table.clear();
        collector.collectHarmonics (table, static_cast<size_t> (waveTableSize));
        return table.getSample (0, 3);
    };

    const auto first = render();
    CHECK (render() == first);
    collector.setControlValues (std::vector<float> { 0.25f, 0.0f });
    CHECK (render() != first);
}

TEST_CASE ("Collector controls follow modulation and clamp to their range", "[tmf_additive_synth][shapes]")
{
    tmf::HarmonicCollectorWithControlsManager manager { tmf::triangleCollectorDefinition() };
    const auto collector = std::dynamic_pointer_cast<tmf::HarmonicCollectorWithControls> (manager.getOrCreateHarmonicCollector (0));
    REQUIRE (collector != nullptr);

    const auto binding = collector->bindModulationTarget ("ShapeTriangle_FLATTEN");
    REQUIRE (binding.has_value());
    CHECK (binding->position().value() == Catch::Approx (0.0f));
    binding->update (0.4f);
    CHECK (collector->getControlValue (tmf::ShapeCollectorControls::Triangle::flatten) == Catch::Approx (0.4f));
    CHECK (binding->position().value() == Catch::Approx (0.4f));
    binding->update (3.0f);
    CHECK (collector->getControlValue (tmf::ShapeCollectorControls::Triangle::flatten) == Catch::Approx (1.0f));

    CHECK (collector->updateModTargetValue ("ShapeTriangle_CURVE", 0.5f));
    CHECK (collector->getEffectiveModulationTargetPosition ("ShapeTriangle_CURVE").value() == Catch::Approx (1.0f));
}

TEST_CASE ("Collector with controls rejects foreign and malformed identities", "[tmf_additive_synth][shapes]")
{
    tmf::HarmonicCollectorWithControlsManager manager { tmf::sawCollectorDefinition() };
    const auto collector = manager.getOrCreateHarmonicCollector (0);

    CHECK_FALSE (collector->updateModTargetValue ("ShapeTriangle_FLATTEN", 0.5f));
    CHECK_FALSE (collector->updateModTargetValue ("ShapeSaw_FLATTEN_extra", 0.5f));
    CHECK_FALSE (collector->bindModulationTarget ("ShapeSaw_UNKNOWN").has_value());
    CHECK_FALSE (collector->getEffectiveModulationTargetPosition ("ShapeSaw_UNKNOWN").has_value());
    CHECK (collector->getEffectiveModulationTargetPosition ("ShapeSaw_LEVEL").has_value());
}

TEST_CASE ("Manager snapshot reaches every collector instance", "[tmf_additive_synth][shapes]")
{
    tmf::HarmonicCollectorWithControlsManager manager { tmf::squareCollectorDefinition() };
    const auto first = std::dynamic_pointer_cast<tmf::HarmonicCollectorWithControls> (manager.getOrCreateHarmonicCollector (0));

    manager.applyParameterSnapshot (std::vector<float> { 0.7f, 5.0f, -20.0f, 0.3f, 0.6f });
    const auto second = std::dynamic_pointer_cast<tmf::HarmonicCollectorWithControls> (manager.getOrCreateHarmonicCollector (1));

    for (const auto& collector : { first, second })
    {
        CHECK (collector->getOrder() == 5);
        CHECK (collector->getControlValue (tmf::ShapeCollectorControls::Square::width) == Catch::Approx (0.3f));
        CHECK (collector->getControlValue (tmf::ShapeCollectorControls::Square::edge) == Catch::Approx (0.6f));
    }
}

TEST_CASE ("Base wave skips hidden collectors and follows the settings", "[tmf_additive_synth][base-wave]")
{
    const auto& definition = tmf::triangleCollectorDefinition();
    tmf::AdditiveBaseWave wave { { makeManager (definition) } };

    const auto hidden = wave.render (makeReader (definition, { { "ShapeTriangle_ORDER", -1.0f } }));
    CHECK (channelMagnitude (hidden, 0) == 0.0f);

    const auto visible = wave.render (makeReader (definition, {}));
    CHECK (channelMagnitude (visible, 0) > 0.9f);

    const auto quieter = wave.render (makeReader (definition, { { "ShapeTriangle_LEVEL", 0.5f } }));
    CHECK (channelMagnitude (quieter, 0) == Catch::Approx (0.5f * channelMagnitude (visible, 0)).margin (0.01));
}

TEST_CASE ("Base wave pans a collector between channels", "[tmf_additive_synth][base-wave]")
{
    const auto& definition = tmf::sawCollectorDefinition();
    const auto cycle = renderCycle (definition, { { "ShapeSaw_PAN", -100.0f } });

    CHECK (channelMagnitude (cycle, 0) > 1.5f);
    CHECK (channelMagnitude (cycle, 1) < 0.001f);
}

TEST_CASE ("Base wave matches the cosine-phase octave harmonics of a colour", "[tmf_additive_synth][base-wave]")
{
    auto colour = std::make_shared<tmf::HarmonicCollectorOctaveHarmonicsManager> (1);
    tmf::AdditiveBaseWave wave { { colour } };
    const auto ids = colour->getAudioParametersIdsStatic (0);
    const auto reader = [&ids] (const std::string& parameterId) {
        if (parameterId == ids[0])
            return 1.0f;

        if (parameterId == ids[1])
            return 0.0f;

        return parameterId.find ("LVL") != std::string::npos ? 1.0f : 0.0f;
    };

    const auto& cycle = wave.render (reader);

    float expectedPeak = 0.0f;
    for (int harmonic = 1; harmonic <= 256; harmonic *= 2)
        expectedPeak += 2.0f / static_cast<float> (harmonic);

    CHECK (cycle.getSample (0, 0) == Catch::Approx (expectedPeak).margin (0.01));
}

TEST_CASE ("Voice output stays at one when parity collectors stack on the same harmonics", "[tmf_additive_synth][wavetable]")
{
    const auto single = renderVoice (1);
    const auto stacked = renderVoice (2);

    REQUIRE (channelMagnitude (single, 0) > 0.1f);
    for (int sample = 0; sample < single.getNumSamples(); ++sample)
        CHECK (stacked.getSample (0, sample) == Catch::Approx (single.getSample (0, sample)).margin (1e-5));
}
