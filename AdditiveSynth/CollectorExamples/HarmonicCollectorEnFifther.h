/*
  ==============================================================================

    HarmonicCollectorEnFifther.h
    Created: 24/5/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "../AdditiveSynthHarmonicCollector.h"
namespace tmf
{
    class HarmonicCollectorEnFifther : public AdditiveSynthHarmonicCollector
    {
    public:
        void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, size_t tableSize) override
        {
            jassert (sampleRate > 0);
            jassert (numChannels > 0);

            int numChannels = audioBuffer.getNumChannels();
            std::vector<float> table = std::vector<float> (tableSize, 0);
            bool overFlowed = false;
            for (int i = 2; !overFlowed; i += 2)
            {
                float power = 0;
                for (int j = 0; j < numChannels; j++)
                {
                    // Nearest fifthHarmonicDirectionUp = i +
                    power += audioBuffer.getReadPointer (j)[i];
                }

                if (power != 0)
                {
                    // We will work with the harmonic index itself
                    // for clarity, instead of the packed format index
                    int harmonicNumber = i / 2;
                    int ascendingFifth;
                    if (harmonicNumber % 2 == 0)
                    {
                        ascendingFifth = (float) harmonicNumber * 1.5;
                    }
                    else
                    {
                        // We look for the next fifth
                        ascendingFifth = (float) harmonicNumber * 1.5 * 2;
                    }

                    int ascendingFifthPackedFormat = ascendingFifth * 2;
                    if (ascendingFifthPackedFormat < tableSize)
                    {
                        table[ascendingFifthPackedFormat] = power / (float) numChannels;
                    }
                    else
                    {
                        overFlowed = true;
                    }
                }
            }
            this->applyPanAndGainAndRenderToBuffer (audioBuffer, table);
        }
    };
}
