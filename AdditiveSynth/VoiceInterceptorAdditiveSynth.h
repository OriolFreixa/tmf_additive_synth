/*
  ==============================================================================

    VoiceInterceptorAdditiveSynth.h
    Created: 15/4/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "tmf_intercept_synth/tmf_intercept_synth.h"
#include "../FFT.h"
#include "AdditiveSynthHarmonicCollector.h"
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
            int) override
        {
            jassert (! juce::approximatelyEqual (sampleRate, 0.0f));
            currentNoteNumber = midiNoteNumber;

            if (pitchWheelValue != 0)
            {
                this->pitchWheelMoved (pitchWheelValue);
            }
            else
            {
                frequency = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber));
                phaseIncrement = (juce::MathConstants<float>::twoPi * frequency) / sampleRate;
            }

            needToRefreshWaveTable = true;

            for (auto& collector : harmonicCollectors)
            {
                collector->setCurrentNote (midiNoteNumber);
            }
        }

        void clearCurrentNote() override 
        {
            currentNoteNumber = -1;
            for (auto& collector : harmonicCollectors)
            {
                collector->setCurrentNote (-1);
            }
        }

        void pitchWheelMoved (int newPitchWheelValue) override
        {
            pitchWheelValue = newPitchWheelValue;
            int numberOfSemitones = 2;
            float valuesPerCent = 8192.0f / ((float) numberOfSemitones * 100.0f);
            newPitchWheelValue -= 8192; //We center the pitchWheelValue
            float cents = newPitchWheelValue / valuesPerCent;
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
        }

        void prepareToPlay (double newSampleRate, int, int numOutputChannels) override
        {
            sampleRate = static_cast<float> (newSampleRate);
            for (auto& collector : harmonicCollectors)
            {
                collector->prepareToPlay (newSampleRate, numOutputChannels);
            }
        }

        void processBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
        {
            if (needToRefreshWaveTable)
            {
                refreshWaveTable();
            }

            auto p = phase; // We reset the phase for each channel
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                p = phase;
                float* channelData = buffer.getWritePointer (channel, startSample);
                for (int sample = 0; sample < numSamples; ++sample)
                {
                    channelData[sample] += getWaveSample(p, channel);
                    p += phaseIncrement;
                    if (p >= juce::MathConstants<float>::twoPi)
                        p -= juce::MathConstants<float>::twoPi;
                }
            }

            for (auto& collector : harmonicCollectors)
            {
                collector->advanceSamples (numSamples);
            }

            phase = p;
        }

        void markWavetableAsNeedsRefresh()
        {
            needToRefreshWaveTable = true;
        }

        void refreshWaveTable()
        {
            if (needToOrderCollectors)
                orderCollectors();

            auto keepRefreshing = collectHarmonics();
            generateWaveTable();
            needToRefreshWaveTable = keepRefreshing;
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

        void updateModTargetValue (juce::String id, float value) override
        {
            auto cachedCollector = modParameterToCollector.find (id);
            if (cachedCollector != modParameterToCollector.end())
            {
                auto collector = cachedCollector->second;
                
                auto hit = collector->updateModTargetValue (id, value);
                jassert (hit);
            }
            else
            {
                searchCollectorAndUpdateModTargetValue (id, value);
            }

            if (id.contains (BaseParameterIdSuffixes::order))
            {
                markCollectorsAsNeedOrder();
            }

            markWavetableAsNeedsRefresh();
        }

    private:
        void searchCollectorAndUpdateModTargetValue (juce::String id, float value)
        {
            for (auto& collector : harmonicCollectors)
            {
                bool found = collector->updateModTargetValue (id, value);
                if (found)
                {
                    modParameterToCollector[id] = collector;
                }
            }
        }
        bool collectHarmonics()
        {
            waveTableBuffer.clear();
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
                collector->collectHarmonics (waveTableBuffer, static_cast<size_t> (jmin (waveTableSize, nyquistTableSize)));
                keepRefreshing = keepRefreshing || collector->waveTableRefreshNeeded();
            }


            return keepRefreshing;
        }

        void generateWaveTable()
        {
            for (int i = 0; i < waveTableBuffer.getNumChannels(); ++i)
            {
                auto* channelData = waveTableBuffer.getWritePointer (i);
                juce::FloatVectorOperations::multiply (channelData, waveTableVolumeMultipliers.data(), waveTableSize);

                fourierTransform.transformRealInverse (channelData);
            }
        }

        float getWaveSample (float samplePhase, int channel)
        {
            jassert(juce::MathConstants<float>::twoPi >= samplePhase);
            jassert(samplePhase >= 0);
            double sample = samplePhase / radiantsPerSample;
            auto waveTable = waveTableBuffer.getReadPointer (channel);

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
        FourierTransform fourierTransform;
        bool needToRefreshWaveTable;

        bool needToOrderCollectors;

        vector<shared_ptr<AdditiveSynthHarmonicCollector>> harmonicCollectors;
        vector<float> waveTableVolumeMultipliers;
        map<juce::String, shared_ptr<AdditiveSynthHarmonicCollector>> modParameterToCollector;
    };
}
