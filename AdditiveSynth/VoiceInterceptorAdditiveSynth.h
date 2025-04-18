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

static constexpr int waveTableOrder = 11;
static constexpr int waveTableSize = 1 << waveTableOrder;
static constexpr double radiantsPerSample = juce::MathConstants<double>::twoPi / (double) (waveTableSize);

namespace tmf
{
    using namespace std;
    class VoiceInterceptorAdditiveSynth : public tmf::VoiceInterceptorGenerator
    {
    public:
        VoiceInterceptorAdditiveSynth() : fourierTransform (waveTableOrder)
        {
            // Initialize the sine wave parameters
            phase = 0.0f;
            phaseIncrement = 0.0f;
            waveTable.fill (0.0f);
            needToRefreshWaveTable = true;
        }

        void startNote (int midiNoteNumber,
            float velocity,
            int currentPitchWheelPosition) override
        {
            jassert (sampleRate != 0.0f);
            frequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
            phaseIncrement = (juce::MathConstants<float>::twoPi * frequency) / sampleRate;

            needToRefreshWaveTable = true;
        }

        void prepareToPlay (double sampleRate, int samplesPerBlock, int numOutputChannels) override
        {
            this->sampleRate = sampleRate;
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
                    channelData[sample] += getWaveSample(p);
                    p += phaseIncrement;
                    if (p >= juce::MathConstants<float>::twoPi)
                        p -= juce::MathConstants<float>::twoPi;
                }
            }

            phase = p;
        }

        void refreshWaveTable()
        {
            collectHarmonics();
            generateWaveTable();
            needToRefreshWaveTable = false;
        }

    private:
        void collectHarmonics()
        {
            waveTable.fill (0.0f);
            waveTable[2] = 1;

            // Remove harms highter than nyquist frequency
        }

        void generateWaveTable()
        {
            fourierTransform.transformRealInverse (waveTable.data());
        }

        float getWaveSample (float phase)
        {
            double sample = phase / radiantsPerSample;

            int firstSample = sample;
            double sampleDecimal = sample - firstSample;
            int secondSample = firstSample + 1;
            if (secondSample == waveTableSize)
                secondSample = 0;
               
            // Read the corresponding values
            jassert (firstSample >= 0 && firstSample < waveTableSize);
            float firstSampleValue = waveTable.at (firstSample);

            jassert (secondSample >= 0 && secondSample < waveTableSize);
            float secondSampleValue = waveTable.at (secondSample);

            // Interpolate the two obtained values
            float sampleValue = firstSampleValue * (1 - sampleDecimal) + secondSampleValue * sampleDecimal;

            return sampleValue;
        }

        float phase, phaseIncrement;
        float frequency;
        float sampleRate;

        array<float, waveTableSize> waveTable;
        FourierTransform fourierTransform;
        bool needToRefreshWaveTable;
    };
}
