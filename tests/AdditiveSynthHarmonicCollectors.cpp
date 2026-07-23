#include "tmf_additive_synth/AdditiveSynth/CollectorExamples/HarmonicCollectorEnFifther.h"
#include "tmf_additive_synth/AdditiveSynth/CollectorExamples/HarmonicCollectorUnevenBands.h"
#include "tmf_additive_synth/AdditiveSynth/CollectorExamples/HarmonicCollectorSine.h"
#include "tmf_additive_synth/FFT.h"
#include "tmf_additive_synth/tmf_additive_synth.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    std::vector<std::string> getDescriptionIds (const std::vector<tmf::SynthParameterDescription>& descriptions)
    {
        std::vector<std::string> ids;
        ids.reserve (descriptions.size());
        for (const auto& description : descriptions)
            ids.push_back (description.id);

        return ids;
    }
}

TEST_CASE ("Harmonic collectors tolerate tables without first harmonic storage", "[tmf_additive_synth][collector]")
{
    juce::AudioBuffer<float> buffer { 2, 2 };
    buffer.clear();

    tmf::HarmonicCollectorSine sine;
    sine.prepareToPlay (44100.0, buffer.getNumChannels());
    sine.collectHarmonics (buffer, 2);

    tmf::HarmonicCollectorEnFifther enFifther;
    enFifther.prepareToPlay (44100.0, buffer.getNumChannels());
    enFifther.collectHarmonics (buffer, 2);

    CHECK (buffer.getSample (0, 0) == 0.0f);
    CHECK (buffer.getSample (0, 1) == 0.0f);
    CHECK (buffer.getSample (1, 0) == 0.0f);
    CHECK (buffer.getSample (1, 1) == 0.0f);
}

TEST_CASE ("Harmonic collector octaves includes the configured high bound", "[tmf_additive_synth][collector]")
{
    juce::AudioBuffer<float> buffer { 2, 16 };
    buffer.clear();

    tmf::HarmonicCollectorOctaves octaves;
    octaves.prepareToPlay (44100.0, buffer.getNumChannels());
    octaves.setParamsValues ({ 1, 3 });
    octaves.collectHarmonics (buffer, 16);

    CHECK (buffer.getSample (0, 2) > 0.0f);
    CHECK (buffer.getSample (0, 4) > 0.0f);
    CHECK (buffer.getSample (0, 8) > 0.0f);
    CHECK (buffer.getSample (1, 2) > 0.0f);
    CHECK (buffer.getSample (1, 4) > 0.0f);
    CHECK (buffer.getSample (1, 8) > 0.0f);
}

TEST_CASE ("Harmonic collector octave start indexes select new interval classes", "[tmf_additive_synth][collector]")
{
    CHECK (tmf::getHarmonicCollectorOctavesStartHarmonic (1) == 1);
    CHECK (tmf::getHarmonicCollectorOctavesStartHarmonic (2) == 3);
    CHECK (tmf::getHarmonicCollectorOctavesStartHarmonic (3) == 5);

    juce::AudioBuffer<float> buffer { 2, 32 };
    buffer.clear();

    tmf::HarmonicCollectorOctaves octaves { 2 };
    octaves.prepareToPlay (44100.0, buffer.getNumChannels());
    octaves.setParamsValues ({ 1, 2 });
    octaves.collectHarmonics (buffer, 32);

    CHECK (buffer.getSample (0, 4) == 0.0f);
    CHECK (buffer.getSample (0, 6) > 0.0f);
    CHECK (buffer.getSample (0, 12) > 0.0f);
    CHECK (std::abs (buffer.getSample (1, 4)) < 0.000001f);
    CHECK (buffer.getSample (1, 6) > 0.0f);
    CHECK (buffer.getSample (1, 12) > 0.0f);
}

TEST_CASE ("Harmonic collector octave harmonics controls each harmonic level and pan", "[tmf_additive_synth][collector]")
{
    juce::AudioBuffer<float> buffer { 2, 16 };
    buffer.clear();

    tmf::HarmonicCollectorOctaveHarmonicsParams params;
    params.levels.fill (0.0f);
    params.pans.fill (0.0f);
    params.levels[1] = 1.0f;
    params.pans[1] = -100.0f;

    tmf::HarmonicCollectorOctaveHarmonics harmonics;
    harmonics.prepareToPlay (44100.0, buffer.getNumChannels());
    harmonics.setParams ({ 1.0f, 0.0f, 0 });
    harmonics.setHarmonicParams (params);
    harmonics.collectHarmonics (buffer, 16);

    CHECK (buffer.getSample (0, 2) == 0.0f);
    CHECK (buffer.getSample (0, 4) > 0.0f);
    CHECK (buffer.getSample (0, 8) == 0.0f);
    CHECK (std::abs (buffer.getSample (1, 4)) < 0.000001f);
}

TEST_CASE ("Harmonic collector noise writes controlled spectral bands", "[tmf_additive_synth][collector]")
{
    juce::AudioBuffer<float> buffer { 2, 140 };
    buffer.clear();

    tmf::HarmonicCollectorUnevenBands noise { 0, 33, 63 };
    noise.prepareToPlay (44100.0, buffer.getNumChannels());
    noise.setParams ({ 1.0f, 0.0f, 0 });
    noise.collectHarmonics (buffer, static_cast<size_t> (buffer.getNumSamples()));

    CHECK (buffer.getSample (0, 64) == 0.0f);
    CHECK (buffer.getSample (0, 66) > 0.0f);
    CHECK (buffer.getSample (0, 67) == 0.0f);
    CHECK (buffer.getSample (0, 126) > 0.0f);
    CHECK (buffer.getSample (0, 130) == 0.0f);
}

TEST_CASE ("Harmonic collector noise selects a single configured high band", "[tmf_additive_synth][collector]")
{
    juce::AudioBuffer<float> buffer { 2, 1024 };
    buffer.clear();

    tmf::HarmonicCollectorUnevenBands noise { 3, 257, 511 };
    noise.prepareToPlay (44100.0, buffer.getNumChannels());
    noise.setParams ({ 1.0f, 0.0f, 0 });
    noise.collectHarmonics (buffer, static_cast<size_t> (buffer.getNumSamples()));

    CHECK (buffer.getSample (0, 510) == 0.0f);
    CHECK (buffer.getSample (0, 514) > 0.0f);
    CHECK (buffer.getSample (0, 515) == 0.0f);
    CHECK (buffer.getSample (0, 1022) > 0.0f);
}

TEST_CASE ("Fourier transform inverse keeps caller buffer bounds", "[tmf_additive_synth][fft]")
{
    constexpr int fftOrder = 3;
    constexpr int fftSize = 1 << fftOrder;
    constexpr int guardSize = 4;
    constexpr int sentinel = 1234;

    std::array<float, fftSize + guardSize> data {};
    data[2] = 1.0f;

    for (int i = fftSize; i < fftSize + guardSize; ++i)
        data[static_cast<size_t> (i)] = static_cast<float> (sentinel);

    FourierTransform fft { fftOrder };
    fft.transformRealInverse (data.data());

    for (int i = fftSize; i < fftSize + guardSize; ++i)
        CHECK (static_cast<int> (data[static_cast<size_t> (i)]) == sentinel);
}

TEST_CASE ("Fourier transform inverse keeps harmonic amplitude audible", "[tmf_additive_synth][fft]")
{
    constexpr int fftOrder = 11;
    constexpr int fftSize = 1 << fftOrder;

    std::array<float, fftSize> data {};
    data[2] = 1.0f;

    FourierTransform fft { fftOrder };
    fft.transformRealInverse (data.data());

    const auto max = *std::max_element (data.begin(), data.end());
    const auto min = *std::min_element (data.begin(), data.end());

    CHECK (max > 0.5f);
    CHECK (min < -0.5f);
}

TEST_CASE ("Harmonic collector manager describes parameters and modulation capability", "[tmf_additive_synth][manager]")
{
    tmf::HarmonicCollectorManager<tmf::HarmonicCollectorSine> manager;

    const auto descriptions = manager.getParameterDescriptions();
    const auto ids = getDescriptionIds (descriptions);
    REQUIRE (ids.size() == 3);

    CHECK (ids[0].ends_with (tmf::BaseParameterIdSuffixes::level));
    CHECK (ids[1].ends_with (tmf::BaseParameterIdSuffixes::order));
    CHECK (ids[2].ends_with (tmf::BaseParameterIdSuffixes::pan));

    for (const auto& description : descriptions)
        CHECK (description.canBeModulationTarget);

    REQUIRE (descriptions[0].groupPath.size() == 1);
    CHECK (descriptions[0].groupPath[0].name.contains ("Sine"));
}

TEST_CASE ("Harmonic collector manager applies parameter changes to active collectors", "[tmf_additive_synth][manager]")
{
    tmf::HarmonicCollectorManager<tmf::HarmonicCollectorSine> manager;
    auto collector = manager.getOrCreateHarmonicCollector (0);
    REQUIRE (collector != nullptr);

    const auto ids = getDescriptionIds (manager.getParameterDescriptions());
    manager.parameterChanged (juce::String { ids[1] }, 7.0f);

    CHECK (manager.getOrder() == 7);
    CHECK (collector->getOrder() == 7);
}

TEST_CASE ("InterceptSynth centrally routes additive collector parameter policy", "[tmf_additive_synth][manager]")
{
    auto collectorManager = std::make_shared<tmf::HarmonicCollectorManager<tmf::HarmonicCollectorSine>>();
    const auto orderId = collectorManager->getId() + tmf::BaseParameterIdSuffixes::order;
    auto policy = tmf::SynthParameterPolicy {};
    policy.hideFromHost (orderId).hideFromMatrix (orderId);

    auto additiveManager = std::make_shared<tmf::VoiceInterceptorAdditiveSynthManager> (
        std::vector<std::shared_ptr<tmf::HarmonicCollectorManagerInterface>> { collectorManager });
    tmf::InterceptSynth synth ({ additiveManager }, policy, 1);

    CHECK (collectorManager->getParameterDescriptions().size() == 3);
    CHECK (synth.getHostParameterDescriptions().size() == 2);
    const auto& targetDescriptions = synth.getModulationTargetDescriptions();
    REQUIRE (targetDescriptions.size() == 130);
    CHECK (targetDescriptions[2].id == tmf::VoiceInterceptorModMatrixConstants::amountParameterId + "0");
}

TEST_CASE ("Additive synth manager routes parameter changes to collector managers", "[tmf_additive_synth][manager]")
{
    auto collectorManager = std::make_shared<tmf::HarmonicCollectorManager<tmf::HarmonicCollectorSine>>();
    const auto orderId = collectorManager->getId() + tmf::BaseParameterIdSuffixes::order;

    tmf::VoiceInterceptorAdditiveSynthManager additiveManager (
        std::vector<std::shared_ptr<tmf::HarmonicCollectorManagerInterface>> { collectorManager });
    additiveManager.parameterChanged (orderId, 9.0f);

    CHECK (collectorManager->getOrder() == 9);
}

TEST_CASE ("Harmonic collector octave harmonics manager exposes per harmonic controls", "[tmf_additive_synth][manager]")
{
    tmf::HarmonicCollectorOctaveHarmonicsManager manager { 2 };

    const auto descriptions = manager.getParameterDescriptions();
    const auto ids = getDescriptionIds (descriptions);
    REQUIRE (ids.size() == 3 + (tmf::maxBoundValue * 2));
    CHECK (ids == tmf::HarmonicCollectorOctaveHarmonicsManager::getAudioParametersIdsStatic (1));
    CHECK (ids[3].ends_with (tmf::HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getLevel (0)));
    CHECK (ids[4].ends_with (tmf::HarmonicCollectorOctaveHarmonicsParameterIdSuffixes::getPan (0)));
    CHECK (std::find (ids.begin(), ids.end(), manager.getId() + tmf::HarmonicCollectorOctavesParameterIdSuffixes::lowBound) == ids.end());
    CHECK (std::find (ids.begin(), ids.end(), manager.getId() + tmf::HarmonicCollectorOctavesParameterIdSuffixes::highBound) == ids.end());

    for (const auto& description : descriptions)
        CHECK (description.canBeModulationTarget);

    REQUIRE (descriptions[0].groupPath.size() == 1);
    CHECK (descriptions[0].groupPath[0].name.contains ("Octave Harmonics"));
}

TEST_CASE ("Harmonic collector noise manager exposes one band collector parameter set", "[tmf_additive_synth][manager]")
{
    tmf::HarmonicCollectorNoiseManager manager { 2, 129, 255 };

    const auto descriptions = manager.getParameterDescriptions();
    const auto ids = getDescriptionIds (descriptions);
    REQUIRE (ids.size() == 3);

    CHECK (ids[0] == tmf::HarmonicCollectorNoiseManager::getLevelParameterIdStatic (2));
    CHECK (ids[0].ends_with (tmf::BaseParameterIdSuffixes::level));
    CHECK (ids[1].ends_with (tmf::BaseParameterIdSuffixes::order));
    CHECK (ids[2].ends_with (tmf::BaseParameterIdSuffixes::pan));

    for (const auto& description : descriptions)
        CHECK (description.canBeModulationTarget);

    REQUIRE (descriptions[0].groupPath.size() == 1);
    CHECK (descriptions[0].groupPath[0].name.contains ("Noise Band"));
}

TEST_CASE ("Harmonic collector noise manager uses the level parameter as collector level", "[tmf_additive_synth][manager]")
{
    tmf::HarmonicCollectorNoiseManager manager { 0, 33, 63 };
    auto collector = std::dynamic_pointer_cast<tmf::HarmonicCollectorUnevenBands> (manager.getOrCreateHarmonicCollector (0));
    REQUIRE (collector != nullptr);

    const auto descriptions = manager.getParameterDescriptions();
    auto levelParameter = descriptions[0].createAudioParameter();
    auto* levelParam = dynamic_cast<juce::AudioParameterFloat*> (levelParameter.get());
    REQUIRE (levelParam != nullptr);
    CHECK (levelParam->get() == 0.0f);

    collector->prepareToPlay (44100.0, 2);

    juce::AudioBuffer<float> buffer { 2, 140 };
    buffer.clear();
    collector->collectHarmonics (buffer, static_cast<size_t> (buffer.getNumSamples()));
    CHECK (buffer.getSample (0, 66) == 0.0f);

    manager.parameterChanged (juce::String { descriptions[0].id }, 1.0f);

    buffer.clear();
    collector->collectHarmonics (buffer, static_cast<size_t> (buffer.getNumSamples()));
    CHECK (buffer.getSample (0, 66) > 0.0f);
}

TEST_CASE ("Harmonic collector noise managers create separate configured bands", "[tmf_additive_synth][manager]")
{
    static constexpr int numNoiseBands = 4;

    for (int i = 0; i < numNoiseBands; ++i)
    {
        const auto firstHarmonic = (32 << i) + 1;
        const auto lastHarmonic = (64 << i) - 1;
        tmf::HarmonicCollectorNoiseManager manager { i, firstHarmonic, lastHarmonic };
        auto collector = std::dynamic_pointer_cast<tmf::HarmonicCollectorUnevenBands> (manager.getOrCreateHarmonicCollector (0));
        REQUIRE (collector != nullptr);
        CHECK (collector->getBandIndex() == i);
    }
}

TEST_CASE ("Additive synth manager renders a configured harmonic collector", "[tmf_additive_synth][voice]")
{
    auto collectorManager = std::make_shared<tmf::HarmonicCollectorManager<tmf::HarmonicCollectorSine>>();
    const auto ids = getDescriptionIds (collectorManager->getParameterDescriptions());
    collectorManager->parameterChanged (juce::String { ids[0] }, 1.0f);
    collectorManager->parameterChanged (juce::String { ids[1] }, 1.0f);

    tmf::VoiceInterceptorAdditiveSynthManager manager { { collectorManager } };
    auto voice = std::dynamic_pointer_cast<tmf::VoiceInterceptorAdditiveSynth> (manager.getOrCreateVoiceInterceptor (0));
    REQUIRE (voice != nullptr);

    voice->prepareToPlay (44100.0, 128, 2);
    voice->startNote (60, 1.0f, 8192);

    juce::AudioBuffer<float> buffer { 2, 128 };
    buffer.clear();
    voice->processBlock (buffer, 0, buffer.getNumSamples());

    auto peak = 0.0f;
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = std::max (peak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));

    CHECK (peak > 0.01f);
}
