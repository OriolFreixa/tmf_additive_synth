/*
  ==============================================================================

    AdditiveBaseWave.h

    Renders the unmodulated stereo cycle the additive synth builds from a set of
    collector settings. It runs the same collectors, saturation, 1/n weighting and
    inverse FFT as a voice, so an editor can draw the real wave without tapping the
    audio thread. Use it from one thread only (normally the message thread).

  ==============================================================================
*/

#pragma once
#include "../FFT.h"
#include "AdditiveSynthHarmonicCollectorManager.h"
#include "AdditiveWavetableMath.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tmf
{
    class AdditiveBaseWave
    {
    public:
        /// Reads the current (denormalised) value of a parameter. Example: `[&apvts] (const std::string& id) { return apvts.getRawParameterValue (id)->load(); }`.
        using ParameterReader = std::function<float (const std::string& parameterId)>;

        /// Takes private collector managers that mirror the synth's own set, in any order.
        /// Example: `AdditiveBaseWave wave { createAdditiveCollectorManagers() }`.
        explicit AdditiveBaseWave (std::vector<std::shared_ptr<HarmonicCollectorManagerInterface>> managers)
            : fourierTransform (waveTableOrder),
              table (2, waveTableSize),
              volumeMultipliers (additiveWavetable::createHarmonicVolumeMultipliers())
        {
            for (auto& manager : managers)
                sources.push_back (createSource (std::move (manager)));
        }

        /// Renders one cycle of both channels, visible collectors only (order is not -1).
        /// Example: `const auto& cycle = wave.render (reader); cycle.getSample (0, 0)`.
        const juce::AudioBuffer<float>& render (const ParameterReader& readParameter)
        {
            table.clear();
            auto activeCollectors = applySettings (readParameter);
            std::stable_sort (activeCollectors.begin(), activeCollectors.end(), [] (const auto& first, const auto& second) {
                return first->getOrder() < second->getOrder();
            });

            for (const auto& collector : activeCollectors)
                collector->collectHarmonics (table, static_cast<size_t> (waveTableSize));

            finishTable();
            return table;
        }

    private:
        struct Source
        {
            std::shared_ptr<HarmonicCollectorManagerInterface> manager;
            std::shared_ptr<AdditiveSynthHarmonicCollector> collector;
            std::vector<std::string> parameterIds;
            std::vector<float> values;
        };

        static Source createSource (std::shared_ptr<HarmonicCollectorManagerInterface> manager)
        {
            Source source;
            for (const auto& description : manager->getParameterDescriptions())
                source.parameterIds.push_back (description.id);

            source.values.resize (source.parameterIds.size());
            source.collector = manager->getOrCreateHarmonicCollector (0);
            // Collectors only need a sample rate to be usable; the base wave itself is rate independent.
            source.collector->prepareToPlay (44100.0, 2);
            source.manager = std::move (manager);
            return source;
        }

        std::vector<std::shared_ptr<AdditiveSynthHarmonicCollector>> applySettings (const ParameterReader& readParameter)
        {
            std::vector<std::shared_ptr<AdditiveSynthHarmonicCollector>> activeCollectors;
            for (auto& source : sources)
            {
                for (size_t index = 0; index < source.values.size(); ++index)
                    source.values[index] = readParameter (source.parameterIds[index]);

                source.manager->applyParameterSnapshot (source.values);
                if (source.collector->getOrder() != -1)
                    activeCollectors.push_back (source.collector);
            }

            return activeCollectors;
        }

        void finishTable()
        {
            additiveWavetable::saturateStereoHarmonics (table);
            for (int channel = 0; channel < table.getNumChannels(); ++channel)
            {
                auto* channelData = table.getWritePointer (channel);
                juce::FloatVectorOperations::multiply (channelData, volumeMultipliers.data(), waveTableSize);
                fourierTransform.transformRealInverse (channelData);
            }
        }

        FourierTransform fourierTransform;
        juce::AudioBuffer<float> table;
        std::vector<float> volumeMultipliers;
        std::vector<Source> sources;
    };
}
