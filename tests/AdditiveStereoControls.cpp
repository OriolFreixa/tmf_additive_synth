#include "tmf_additive_synth/tmf_additive_synth.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{
    using StereoValues = std::array<float, tmf::harmonicStereo::controlCount>;

    juce::AudioBuffer<float> stereoSpectrum (const tmf::ControlledCollectorDefinition& definition,
        StereoValues stereo = { 0, 0, 8 },
        float pan = 0,
        size_t entryCount = waveTableSize)
    {
        tmf::HarmonicCollectorWithControls collector { definition };
        collector.prepareToPlay (44100, 2);
        collector.setParams ({ 1, pan, 0 });
        collector.setStereoControlValues (stereo);
        juce::AudioBuffer<float> table { 2, waveTableSize };
        table.clear();
        collector.collectHarmonics (table, entryCount);
        return table;
    }

    float spectrumPan (const juce::AudioBuffer<float>& spectrum, size_t harmonic)
    {
        const int entry = static_cast<int> (2 * harmonic);
        const float left = std::hypot (spectrum.getSample (0, entry), spectrum.getSample (0, entry + 1));
        const float right = std::hypot (spectrum.getSample (1, entry), spectrum.getSample (1, entry + 1));
        REQUIRE (left + right > 1.0e-6f);
        return 100.0f * (right - left) / (right + left);
    }

    std::vector<float> defaultTimbreSpectrum (const tmf::ControlledCollectorDefinition& definition)
    {
        std::vector<float> values;
        for (const auto& control : definition.controls)
            values.push_back (control.defaultValue);
        std::vector<float> spectrum (waveTableSize, 0);
        definition.buildSpectrum (values, spectrum);
        return spectrum;
    }

    std::vector<float> fullStereoSnapshot (const tmf::ControlledCollectorDefinition& definition, StereoValues stereo)
    {
        std::vector<float> values { 1, 0, 0 };
        for (const auto& control : definition.controls)
            values.push_back (control.defaultValue);
        values.insert (values.end(), stereo.begin(), stereo.end());
        return values;
    }
}

TEST_CASE ("Neutral stereo controls preserve shape and parity spectra and source pan", "[tmf_additive_synth][stereo]")
{
    const auto* definition = GENERATE (&tmf::triangleCollectorDefinition(), &tmf::sawCollectorDefinition(), &tmf::squareCollectorDefinition(), &tmf::unevenCollectorDefinition(), &tmf::evenCollectorDefinition());
    INFO (definition->id);
    const auto expected = defaultTimbreSpectrum (*definition);
    const auto actual = stereoSpectrum (*definition, { 0, 0, 8 }, -35);
    for (int entry = 0; entry < waveTableSize; ++entry)
    {
        CHECK (actual.getSample (0, entry) == Catch::Approx (1.35f * expected[static_cast<size_t> (entry)]).margin (1e-6));
        CHECK (actual.getSample (1, entry) == Catch::Approx (0.65f * expected[static_cast<size_t> (entry)]).margin (1e-6));
    }
}

TEST_CASE ("Spread alternates active harmonics within each parity source", "[tmf_additive_synth][stereo]")
{
    const auto* definition = GENERATE (&tmf::unevenCollectorDefinition(), &tmf::evenCollectorDefinition());
    const size_t first = definition == &tmf::unevenCollectorDefinition() ? 1 : 2;
    const auto positive = stereoSpectrum (*definition, { 100, 0, 8 });
    const auto negative = stereoSpectrum (*definition, { -100, 0, 8 });
    CHECK (spectrumPan (positive, first) == Catch::Approx (-100));
    CHECK (spectrumPan (positive, first + 2) == Catch::Approx (100));
    CHECK (spectrumPan (positive, first + 4) == Catch::Approx (-100));
    CHECK (spectrumPan (negative, first) == Catch::Approx (100));
    CHECK (spectrumPan (negative, first + 2) == Catch::Approx (-100));
}

TEST_CASE ("Spread ignores absent harmonics and FFT residue", "[tmf_additive_synth][stereo]")
{
    std::vector<float> spectrum (14, 0);
    spectrum[3] = 1;
    spectrum[5] = 1e-8f;
    spectrum[6] = -0.3f;
    spectrum[11] = 0.1f;
    const auto directions = tmf::harmonicStereo::spreadDirections (spectrum);
    CHECK (directions[1] == -1);
    CHECK (directions[2] == 0);
    CHECK (directions[3] == 1);
    CHECK (directions[4] == 0);
    CHECK (directions[5] == -1);
}

TEST_CASE ("Default square and triangle spread ignore even-harmonic numerical residue", "[tmf_additive_synth][stereo]")
{
    const auto* definition = GENERATE (&tmf::triangleCollectorDefinition(), &tmf::squareCollectorDefinition());
    const auto spectrum = defaultTimbreSpectrum (*definition);
    const auto directions = tmf::harmonicStereo::spreadDirections (spectrum);
    for (size_t harmonic = 1; harmonic <= 511; ++harmonic)
    {
        INFO (harmonic);
        const float expected = harmonic % 2 == 0 ? 0.0f : (harmonic % 4 == 1 ? -1.0f : 1.0f);
        CHECK (directions[harmonic] == expected);
    }
}

TEST_CASE ("Tilt uses a logarithmic harmonic pivot and reverses with its sign", "[tmf_additive_synth][stereo]")
{
    const auto& definition = tmf::sawCollectorDefinition();
    const auto tilted = stereoSpectrum (definition, { 0, 100, 8 });
    CHECK (spectrumPan (tilted, 4) == Catch::Approx (-25));
    CHECK (spectrumPan (tilted, 8) == Catch::Approx (0).margin (1e-5));
    CHECK (spectrumPan (tilted, 16) == Catch::Approx (25));
    const auto reversed = stereoSpectrum (definition, { 0, -100, 8 });
    CHECK (spectrumPan (reversed, 4) == Catch::Approx (25));
    CHECK (spectrumPan (reversed, 16) == Catch::Approx (-25));
    const auto moved = stereoSpectrum (definition, { 0, 100, 16 });
    CHECK (spectrumPan (moved, 8) == Catch::Approx (-25));
    CHECK (spectrumPan (moved, 16) == Catch::Approx (0).margin (1e-5));
    CHECK (tmf::harmonicStereo::tiltPosition (1, 512) == -1);
    CHECK (tmf::harmonicStereo::tiltPosition (512, 1) == 1);
}

TEST_CASE ("Stereo offsets combine with source pan and keep the mono spectrum", "[tmf_additive_synth][stereo]")
{
    CHECK (tmf::harmonicStereo::harmonicPan (8, 1, 80, 50, 100, 8) == 100);
    CHECK (tmf::harmonicStereo::harmonicPan (8, -1, -80, 50, 100, 8) == -100);
    const auto& definition = tmf::sawCollectorDefinition();
    const auto expected = defaultTimbreSpectrum (definition);
    const auto stereo = stereoSpectrum (definition, { -67, 81, 12 }, 23);
    for (int entry = 0; entry < waveTableSize; ++entry)
        CHECK (0.5f * (stereo.getSample (0, entry) + stereo.getSample (1, entry))
               == Catch::Approx (expected[static_cast<size_t> (entry)]).margin (1e-6));
}

TEST_CASE ("Stereo modulation binds by identity and clamps the full parameter span", "[tmf_additive_synth][stereo]")
{
    const auto& definition = tmf::sawCollectorDefinition();
    tmf::HarmonicCollectorWithControlsManager manager { definition };
    const auto collector = manager.getOrCreateHarmonicCollector (0);
    for (size_t index = 0; index < tmf::harmonicStereo::controlCount; ++index)
    {
        const auto id = definition.stereoControlId (index);
        auto binding = collector->bindModulationTarget (id);
        REQUIRE (binding.has_value());
        binding->update (3);
        CHECK (binding->position().value() == Catch::Approx (1));
        CHECK (collector->updateModTargetValue (id, -3));
        CHECK (collector->getEffectiveModulationTargetPosition (id).value() == Catch::Approx (0));
        CHECK_FALSE (collector->bindModulationTarget (id + "_extra").has_value());
    }
}

TEST_CASE ("Stereo snapshots reach existing and future collectors and legacy snapshots reset them", "[tmf_additive_synth][stereo]")
{
    const auto& definition = tmf::squareCollectorDefinition();
    tmf::HarmonicCollectorWithControlsManager manager { definition };
    const auto first = manager.getOrCreateHarmonicCollector (0);
    manager.applyParameterSnapshot (fullStereoSnapshot (definition, { 60, -40, 32 }));
    const auto second = manager.getOrCreateHarmonicCollector (1);
    for (const auto& collector : { first, second })
        CHECK (collector->getEffectiveModulationTargetPosition (definition.stereoControlId (0)).value() == Catch::Approx (0.8f));
    auto legacy = fullStereoSnapshot (definition, { 0, 0, 8 });
    legacy.resize (3 + definition.controls.size());
    manager.applyParameterSnapshot (legacy);
    for (const auto& collector : { first, second })
        CHECK (collector->getEffectiveModulationTargetPosition (definition.stereoControlId (0)).value() == Catch::Approx (0.5f));
}

TEST_CASE ("Stereo rendering respects partial packed tables", "[tmf_additive_synth][stereo]")
{
    const auto count = GENERATE (size_t { 0 }, size_t { 1 }, size_t { 2 }, size_t { 3 }, size_t { 4 }, size_t { 17 });
    const auto spectrum = stereoSpectrum (tmf::sawCollectorDefinition(), { 65, 34, 8 }, 0, count);
    for (int entry = static_cast<int> (count); entry < spectrum.getNumSamples(); ++entry)
    {
        CHECK (spectrum.getSample (0, entry) == 0);
        CHECK (spectrum.getSample (1, entry) == 0);
    }
}

TEST_CASE ("Stereo control changes reuse the cached timbre spectrum", "[tmf_additive_synth][stereo]")
{
    static int builds = 0;
    builds = 0;
    const tmf::ControlledCollectorDefinition definition {
        "StereoCache", "Stereo Cache", {}, {}, [] (std::span<const float>, std::span<float> spectrum) { ++builds; spectrum[3] = 1; }
    };
    tmf::HarmonicCollectorWithControls collector { definition };
    juce::AudioBuffer<float> table { 2, waveTableSize };
    table.clear();
    collector.collectHarmonics (table, waveTableSize);
    collector.setStereoControlValues (StereoValues { 40, 70, 16 });
    collector.collectHarmonics (table, waveTableSize);
    CHECK (builds == 1);
}
