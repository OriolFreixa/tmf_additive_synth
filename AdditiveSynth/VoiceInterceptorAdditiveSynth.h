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
            auto keepRefreshing = collectHarmonics();
            generateWaveTable();
            installGeneratedWaveTable();
            pitchRefreshPending = false;
            needToRefreshWaveTable = keepRefreshing;
            // Coalesce control ticks while retaining the newest collector targets.
            // A full fade interval bounds FFT work independently of host subdivisions.
            tableRefreshCountdown = firstTable ? 0 : transitionLength;
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
            // Share phase and fade progress across channels to preserve stereo alignment.
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const auto blend = static_cast<float> (transitionSample) / static_cast<float> (transitionLength);
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    buffer.getWritePointer (channel, startSample)[sample] += getTransitionSample (phase, channel, blend);

                transitionSample = std::min (transitionLength, transitionSample + 1);
                phase += phaseIncrement;
                if (phase >= juce::MathConstants<float>::twoPi)
                    phase -= juce::MathConstants<float>::twoPi;
            }
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

        float getTransitionSample (float samplePhase, int channel, float blend) const
        {
            const auto target = getWaveSample (waveTableBuffer, samplePhase, channel);
            if (transitionSample == transitionLength)
                return target;
            const auto source = getWaveSample (transitionSourceTable, samplePhase, channel);
            return source + blend * (target - source);
        }

        static float getWaveSample (const juce::AudioBuffer<float>& table, float samplePhase, int channel)
        {
            jassert (juce::MathConstants<float>::twoPi >= samplePhase);
            jassert (samplePhase >= 0);
            double sample = samplePhase / radiantsPerSample;
            auto waveTable = table.getReadPointer (channel);

            auto firstSample = static_cast<int> (sample);
            double sampleDecimal = sample - firstSample;
            int secondSample = firstSample + 1;
            if (secondSample == waveTableSize)
                secondSample = 0;

            // Read the corresponding values
            jassert (firstSample >= 0 && firstSample < waveTableSize);
            float firstSampleValue = waveTable[firstSample];

            jassert (secondSample >= 0 && secondSample < waveTableSize);
            float secondSampleValue = waveTable[secondSample];

            // Interpolate the two obtained values
            auto sampleValue = static_cast<float> (firstSampleValue * (1.0 - sampleDecimal) + secondSampleValue * sampleDecimal);

            return sampleValue;
        }

        int currentNoteNumber;
        float phase, phaseIncrement;
        float frequency;
        float sampleRate = 0;

        int pitchWheelValue;

        juce::AudioBuffer<float> waveTableBuffer;
        juce::AudioBuffer<float> transitionSourceTable;
        juce::AudioBuffer<float> generatedWaveTable;
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
