/*
  ==============================================================================

    VoiceInterceptorAdditiveSynth.h
    Created: 15/4/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "../FFT.h"
#include "AdditiveSynthHarmonicCollector.h"
#include "tmf_intercept_synth/tmf_intercept_synth.h"
#include <algorithm>

static constexpr int waveTableOrder = 11;
static constexpr int waveTableSize = 1 << waveTableOrder;
static constexpr double radiantsPerSample = juce::MathConstants<double>::twoPi / (double) (waveTableSize);

namespace tmf
{
    /// <summary>
    /// Added additive synth module. Additive synth is a generator interceptor.
    /// It works by "collecting" harmonics from harmonic collectors, adding them
    /// together and printing them to the buffer.
    /// A harmonic collector si a class which prints a set of harmonics to the
    /// waveTable (preferebly, it takes in account the contents of the table, printed
    /// by other harmonic collectors)
    /// </summary>
    using namespace std;
    class VoiceInterceptorAdditiveSynth : public tmf::VoiceInterceptorGenerator, public VoiceInterceptorWithModTargets
    {
    public:
        VoiceInterceptorAdditiveSynth()
            : currentNoteNumber (-1),
              phase (0.0f),
              phaseIncrement (0.0f),
              frequency (0.0f),
              sampleRate (0.0f),
              pitchWheelValue (0.0f),
              waveTableBuffer (2, waveTableSize),
              transitionSourceTable (2, waveTableSize),
              generatedWaveTable (2, waveTableSize),
              fourierTransform (waveTableOrder),
              needToRefreshWaveTable (true),
              needToOrderCollectors (true)
        {
            waveTableBuffer.clear();

            waveTableVolumeMultipliers = vector<float> (waveTableSize, 1.0f);

            // We start at the first partial, and skip the phase indexs (odd)
            for (int i = 2; i < waveTableSize; i += 2)
            {
                waveTableVolumeMultipliers[static_cast<size_t> (i)] = 1.0f / static_cast<float> (i / 2);
            }
        }

        /// Assigns a repeatable refresh phase before preparation. Example: manager voice one uses the half-period slot.
        void setTableRefreshVoiceIndex (size_t voiceIndex)
        {
            tableRefreshVoiceIndex = voiceIndex;
        }

        void addHarmonicCollector (shared_ptr<AdditiveSynthHarmonicCollector> collector)
        {
            jassert (juce::approximatelyEqual (sampleRate, 0.0f)); // Collectors must be created before starting the synth
            harmonicCollectors.push_back (collector);
        }

        void startNote (int midiNoteNumber,
            float,
            int currentPitchWheelPosition) override
        {
            jassert (!juce::approximatelyEqual (sampleRate, 0.0f));
            currentNoteNumber = midiNoteNumber;
            hasPlaybackTable = false;
            tableRefreshCountdown = 0;
            transitionSample = transitionLength;

            pitchWheelMoved (currentPitchWheelPosition);

            needToRefreshWaveTable = true;

            for (auto& collector : harmonicCollectors)
            {
                collector->setCurrentNote (midiNoteNumber);
            }
        }

        void clearCurrentNote() override
        {
            currentNoteNumber = -1;
            hasPlaybackTable = false;
            tableRefreshCountdown = 0;
            transitionSample = transitionLength;
            for (auto& collector : harmonicCollectors)
            {
                collector->setCurrentNote (-1);
            }
        }

        void pitchWheelMoved (int newPitchWheelValue) override
        {
            // Retain immediate pitch refreshes so an upward bend cannot fade out
            // partials that have already crossed the new Nyquist cutoff.
            pitchRefreshPending = pitchRefreshPending || pitchWheelValue != newPitchWheelValue;
            pitchWheelValue = newPitchWheelValue;
            int numberOfSemitones = 2;
            float valuesPerCent = 8192.0f / ((float) numberOfSemitones * 100.0f);
            newPitchWheelValue -= 8192; //We center the pitchWheelValue
            float cents = static_cast<float> (newPitchWheelValue) / valuesPerCent;
            if (newPitchWheelValue > 0)
            {
                auto hzPerCent = static_cast<float> ((juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber + 1) - juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber)) / 100.0);
                frequency = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber)) + hzPerCent * cents;
                phaseIncrement = (juce::MathConstants<float>::twoPi * frequency) / sampleRate;
            }
            else
            {
                auto hzPerCent = static_cast<float> ((juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber - 1) - juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber)) / 100.0);
                frequency = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber)) - hzPerCent * cents;
                phaseIncrement = (juce::MathConstants<float>::twoPi * frequency) / sampleRate;
            }

            // The packed-spectrum cutoff is Nyquist / frequency. Retuning playback
            // without rebuilding leaves partials that have crossed Nyquist in the
            // table, and a downward bend keeps the tighter cutoff of the old pitch.
            markWavetableAsNeedsRefresh();
        }

        void prepareToPlay (double newSampleRate, int, int numOutputChannels) override
        {
            sampleRate = static_cast<float> (newSampleRate);
            transitionLength = std::max (1, static_cast<int> (std::round (newSampleRate * 0.01)));
            transitionSample = transitionLength;
            hasPlaybackTable = false;
            tableRefreshCountdown = 0;
            needToRefreshWaveTable = true;
            for (auto& collector : harmonicCollectors)
            {
                collector->prepareToPlay (newSampleRate, numOutputChannels);
            }
        }

        void processBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
        {
            while (numSamples > 0)
            {
                if (needToRefreshWaveTable && (tableRefreshCountdown == 0 || pitchRefreshPending || !hasPlaybackTable))
                    refreshWaveTable();

                // Keep phased clocks advancing while clean, so later automation cannot synchronize every voice.
                if (!needToRefreshWaveTable && tableRefreshCountdown == 0 && tableRefreshVoiceIndex != 0)
                    tableRefreshCountdown = transitionLength;

                const auto sliceSamples = tableRefreshCountdown > 0 ? std::min (numSamples, tableRefreshCountdown) : numSamples;
                renderTableSpan (buffer, startSample, sliceSamples);
                for (auto& collector : harmonicCollectors)
                    collector->advanceSamples (sliceSamples);

                tableRefreshCountdown = std::max (0, tableRefreshCountdown - sliceSamples);
                startSample += sliceSamples;
                numSamples -= sliceSamples;
            }
        }

        void markWavetableAsNeedsRefresh()
        {
            needToRefreshWaveTable = true;
        }

        void refreshWaveTable()
        {
            if (needToOrderCollectors)
                orderCollectors();

            const auto firstTable = !hasPlaybackTable;
            const auto previousCountdown = tableRefreshCountdown;
            auto keepRefreshing = collectHarmonics();
            generateWaveTable();
            installGeneratedWaveTable();
            pitchRefreshPending = false;
            needToRefreshWaveTable = keepRefreshing;
            // Coalesce control ticks while retaining the newest collector targets.
            // A full fade interval bounds FFT work independently of host subdivisions.
            // Urgent pitch rebuilds retain the periodic deadline instead of synchronizing all voices again.
            const auto retainDeadline = tableRefreshVoiceIndex != 0 && previousCountdown > 0;
            const auto initialDelay = initialTableRefreshDelay();
            // A smoothing first table must wait a full interval rather than rebuilding on the next host slice.
            tableRefreshCountdown = firstTable ? (initialDelay == 0 && keepRefreshing ? transitionLength : initialDelay)
                                               : (retainDeadline ? previousCountdown : transitionLength);
        }

        void markCollectorsAsNeedOrder()
        {
            needToOrderCollectors = true;
        }

        void orderCollectors()
        {
            std::sort (
                this->harmonicCollectors.begin(),
                this->harmonicCollectors.end(),
                [] (const shared_ptr<AdditiveSynthHarmonicCollector>& a, const shared_ptr<AdditiveSynthHarmonicCollector>& b) {
                    return a->getOrder() < b->getOrder();
                });

            needToOrderCollectors = false;
        }

        /// Binds a collector once before rendering. Example: matrix ticks call the cached harmonic setter directly.
        ModulationTargetBinding bindModulationTarget (const juce::String& id) override
        {
            for (const auto& collector : harmonicCollectors)
            {
                // Full collector prefixes avoid scanning unrelated harmonics during graph setup.
                if (!id.startsWith (juce::String (collector->getId()) + "_"))
                    continue;
                auto binding = collector->bindModulationTarget (id);
                if (!binding)
                    continue;
                return { [this, collector, binding = *binding] (float value) {
                            const auto previousPosition = binding.position();
                            const auto previousOrder = collector->getOrder();
                            // Retain raw offsets even when clamping hides their audible effect.
                            binding.update (value);
                            if (previousPosition && previousPosition == binding.position())
                                return;
                            if (previousOrder != collector->getOrder())
                                markCollectorsAsNeedOrder();
                            markWavetableAsNeedsRefresh();
                        },
                    binding->position };
            }
            return VoiceInterceptorWithModTargets::bindModulationTarget (id);
        }

        void updateModTargetValue (juce::String id, float value) override
        {
            const auto cachedCollector = modParameterToCollector.find (id);
            if (cachedCollector != modParameterToCollector.end())
            {
                applyCollectorModulation (*cachedCollector->second, id, value);
                return;
            }

            for (const auto& collector : harmonicCollectors)
            {
                if (!applyCollectorModulation (*collector, id, value))
                    continue;
                modParameterToCollector[id] = collector;
                return;
            }
        }

        std::optional<float> getEffectiveModulationTargetPosition (
            const juce::String& id) const noexcept override
        {
            for (const auto& collector : harmonicCollectors)
                if (const auto position = collector->getEffectiveModulationTargetPosition (id))
                    return position;

            return std::nullopt;
        }

    private:
        void renderTableSpan (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
        {
            const auto channels = std::min (2, buffer.getNumChannels());
            float* output[2] { channels > 0 ? buffer.getWritePointer (0, startSample) : nullptr, channels > 1 ? buffer.getWritePointer (1, startSample) : nullptr };
            const float* target[2] { waveTableBuffer.getReadPointer (0), waveTableBuffer.getReadPointer (1) };
            const float* source[2] { transitionSourceTable.getReadPointer (0), transitionSourceTable.getReadPointer (1) };
            // Share phase, interpolation indices and fade progress across both channels and tables.
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const double position = phase / radiantsPerSample;
                const int first = static_cast<int> (position);
                const int second = (first + 1) % waveTableSize;
                const double fraction = position - first;
                jassert (first >= 0 && first < waveTableSize);
                const bool fading = transitionSample < transitionLength;
                const float blend = fading ? static_cast<float> (transitionSample) / static_cast<float> (transitionLength) : 1.0f;
                for (int channel = 0; channel < channels; ++channel)
                {
                    const float next = static_cast<float> (target[channel][first] * (1.0 - fraction) + target[channel][second] * fraction);
                    const float previous = fading ? static_cast<float> (source[channel][first] * (1.0 - fraction) + source[channel][second] * fraction) : next;
                    output[channel][sample] += fading ? previous + blend * (next - previous) : next;
                }
                transitionSample += fading ? 1 : 0;
                phase += phaseIncrement;
                if (phase >= juce::MathConstants<float>::twoPi)
                    phase -= juce::MathConstants<float>::twoPi;
            }
        }

        int initialTableRefreshDelay() const
        {
            // Bit reversal spreads the first 2, 4, 8, ... voices evenly without requiring a fixed voice count.
            double fraction = 0.0, place = 0.5;
            for (auto index = tableRefreshVoiceIndex; index != 0; index >>= 1, place *= 0.5)
                fraction += static_cast<double> (index & 1u) * place;
            if (tableRefreshVoiceIndex == 0)
                return 0;
            return juce::jlimit (1, transitionLength, static_cast<int> (std::round (fraction * transitionLength)));
        }

        bool applyCollectorModulation (AdditiveSynthHarmonicCollector& collector,
            const juce::String& id,
            float value)
        {
            const auto previousPosition = collector.getEffectiveModulationTargetPosition (id);
            const auto previousOrder = collector.getOrder();
            // Retain the raw offset even when clamping or quantization hides its effect.
            if (!collector.updateModTargetValue (id, value))
                return false;
            const auto currentPosition = collector.getEffectiveModulationTargetPosition (id);
            if (previousPosition && currentPosition && previousPosition == currentPosition)
                return true;

            if (previousOrder != collector.getOrder())
                markCollectorsAsNeedOrder();
            markWavetableAsNeedsRefresh();
            return true;
        }
        bool collectHarmonics()
        {
            generatedWaveTable.clear();
            auto keepRefreshing = false;

            auto nyquistFrequency = static_cast<int> (sampleRate / 2.0f);
            auto nyquistHarmonic = static_cast<int> (static_cast<float> (nyquistFrequency) / frequency);
            for (auto& collector : harmonicCollectors)
            {
                if (collector->getOrder() == -1)
                    continue;

                // The table has two entries for each harmonic (amplitude and phase)
                // The first entries is reserved for DC component
                // The second entry is for the last real nyquistHarmonic, which shouldn't be used
                auto nyquistTableSize = (nyquistHarmonic * 2) + 2;
                collector->collectHarmonics (generatedWaveTable, static_cast<size_t> (jmin (waveTableSize, nyquistTableSize)));
                keepRefreshing = keepRefreshing || collector->waveTableRefreshNeeded();
            }

            return keepRefreshing;
        }

        void generateWaveTable()
        {
            for (int i = 0; i < generatedWaveTable.getNumChannels(); ++i)
            {
                auto* channelData = generatedWaveTable.getWritePointer (i);
                juce::FloatVectorOperations::multiply (channelData, waveTableVolumeMultipliers.data(), waveTableSize);

                fourierTransform.transformRealInverse (channelData);
            }
        }

        bool generatedTableMatchesTarget() const
        {
            for (int channel = 0; channel < 2; ++channel)
                if (!std::equal (generatedWaveTable.getReadPointer (channel),
                        generatedWaveTable.getReadPointer (channel) + waveTableSize,
                        waveTableBuffer.getReadPointer (channel)))
                    return false;
            return true;
        }

        void installGeneratedWaveTable()
        {
            if (hasPlaybackTable && !pitchRefreshPending && generatedTableMatchesTarget())
                return;

            const auto crossfade = hasPlaybackTable && !pitchRefreshPending;
            if (crossfade)
                captureTransitionSource();
            std::swap (waveTableBuffer, generatedWaveTable);
            transitionSample = crossfade ? 0 : transitionLength;
            hasPlaybackTable = true;
        }

        void captureTransitionSource()
        {
            // Retarget from the waveform currently audible, including an unfinished fade.
            const auto blend = static_cast<float> (transitionSample) / static_cast<float> (transitionLength);
            for (int channel = 0; channel < 2; ++channel)
            {
                auto* source = transitionSourceTable.getWritePointer (channel);
                const auto* target = waveTableBuffer.getReadPointer (channel);
                for (int sample = 0; sample < waveTableSize; ++sample)
                    source[sample] = transitionSample == transitionLength ? target[sample] : source[sample] + blend * (target[sample] - source[sample]);
            }
        }

        int currentNoteNumber;
        float phase, phaseIncrement;
        float frequency;
        float sampleRate = 0;

        int pitchWheelValue;

        juce::AudioBuffer<float> waveTableBuffer;
        juce::AudioBuffer<float> transitionSourceTable;
        juce::AudioBuffer<float> generatedWaveTable;
        size_t tableRefreshVoiceIndex = 0;
        int tableRefreshCountdown = 0;
        int transitionLength = 1;
        int transitionSample = 1;
        bool hasPlaybackTable = false;
        bool pitchRefreshPending = false;
        FourierTransform fourierTransform;
        bool needToRefreshWaveTable;

        bool needToOrderCollectors;

        vector<shared_ptr<AdditiveSynthHarmonicCollector>> harmonicCollectors;
        vector<float> waveTableVolumeMultipliers;
        map<juce::String, shared_ptr<AdditiveSynthHarmonicCollector>> modParameterToCollector;
    };
}
