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
    class VoiceInterceptorAdditiveSynth : public tmf::VoiceInterceptorGenerator
    {
    public:
        VoiceInterceptorAdditiveSynth() : fourierTransform (waveTableOrder), frequency (0.0f), sampleRate (0.0f)
        {
            phase = 0.0f;
            phaseIncrement = 0.0f;
            waveTableBuffer.clear();
            needToRefreshWaveTable = true;
            needToOrderCollectors = true;

            waveTableVolumeMultipliers = vector<float> (waveTableSize, 1.0f);

            // We start at the first partial, and skip the phase indexs (odd)
            for (int i = 2; i < waveTableSize; i += 2)
            {
                waveTableVolumeMultipliers[i] = 1.0f / (i / 2);
            }
        }

        void addHarmonicCollector (shared_ptr<AdditiveSynthHarmonicCollector> collector)
        {
            jassert (sampleRate == 0.0f); // Collectors must be created before starting the synth
            harmonicCollectors.push_back (collector);
        }

        void startNote (int midiNoteNumber,
            float velocity,
            int currentPitchWheelPosition) override
        {
            jassert (sampleRate != 0.0f);
            frequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
            phaseIncrement = (juce::MathConstants<float>::twoPi * frequency) / sampleRate;

            needToRefreshWaveTable = true;

            for (auto& collector : harmonicCollectors)
            {
                collector->setCurrentNote (midiNoteNumber);
            }
        }

        void clearCurrentNote() override 
        {
            for (auto& collector : harmonicCollectors)
            {
                collector->setCurrentNote (-1);
            }
        }

        void prepareToPlay (double sampleRate, int samplesPerBlock, int numOutputChannels) override
        {
            this->sampleRate = sampleRate;
            for (auto& collector : harmonicCollectors)
            {
                collector->prepareToPlay (sampleRate, numOutputChannels);
            }
        }

        void processBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
        {
            if (needToRefreshWaveTable)
            {
                refreshWaveTable();
            }

            float p; // We reset the phase for each channel
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

        /* TODO: Pitch wheel moved
        int numberOfSemitones = 2;
        float valuesPerCent = 8192.0f / ((float) numberOfSemitones * 100.0f);
        newPitchWheelValue -= 8192; //We center the pitchWheelValue
        float cents = newPitchWheelValue / valuesPerCent;
        if (newPitchWheelValue > 0)
        {
            float hzPerCent = (juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber + 1) - juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber)) / 100.0f;
            osc.setFrequency (juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber) + hzPerCent * cents);
        }
        else
        {
            float hzPerCent = (juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber - 1) - juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber)) / 100.0f;
            osc.setFrequency ((float) juce::MidiMessage::getMidiNoteInHertz (currentNoteNumber) - hzPerCent * cents);
        }
        */

    private:
        bool collectHarmonics()
        {
            waveTableBuffer.clear();
            auto keepRefreshing = false;

            int nyquistFrequency = sampleRate / 2;
            int nyquistHarmonic = (int) (nyquistFrequency / frequency);
            for (auto& collector : harmonicCollectors)
            {
                if (collector->getOrder() == -1)
                    continue;

                // The table has two entries for each harmonic (amplitude and phase)
                // The first entries is reserved for DC component
                // The second entry is for the last real nyquistHarmonic, which shouldn't be used
                auto nyquistTableSize = (nyquistHarmonic * 2) + 2;
                collector->collectHarmonics (waveTableBuffer, jmin (waveTableSize, nyquistTableSize));
                keepRefreshing = keepRefreshing || collector->waveTableRefreshNeeded();
            }


            return keepRefreshing;
            // TODO: Remove harms highter than nyquist frequency
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

        float getWaveSample (float phase, int channel)
        {
            jassert(juce::MathConstants<float>::twoPi >= phase);
            jassert(phase >= 0);
            double sample = phase / radiantsPerSample;
            auto waveTable = waveTableBuffer.getReadPointer (channel);

            int firstSample = sample;
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
            float sampleValue = firstSampleValue * (1 - sampleDecimal) + secondSampleValue * sampleDecimal;

            return sampleValue;
        }

        float phase, phaseIncrement;
        float frequency;
        float sampleRate = 0;

        juce::AudioBuffer<float> waveTableBuffer = juce::AudioBuffer<float>(2, waveTableSize);
        FourierTransform fourierTransform;
        bool needToRefreshWaveTable;

        bool needToOrderCollectors;

        vector<shared_ptr<AdditiveSynthHarmonicCollector>> harmonicCollectors;
        vector<float> waveTableVolumeMultipliers;
    };
}
