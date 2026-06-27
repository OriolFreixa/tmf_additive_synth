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
            std::vector<float> table = std::vector<float> (tableSize, 0.0f);
            auto tableSizeAsInt = static_cast<int> (tableSize);
            for (int i = 2; i < tableSizeAsInt; i += 2)
            {
                float power = 0.0f;
                for (int j = 0; j < numChannels; j++)
                {
                    // Nearest fifthHarmonicDirectionUp = i +
                    power += audioBuffer.getReadPointer (j)[i];
                }

                if (! juce::approximatelyEqual (power, 0.0f))
                {
                    // We will work with the harmonic index itself
                    // for clarity, instead of the packed format index
                    int harmonicNumber = i / 2;
                    int ascendingFifth;
                    if (harmonicNumber % 2 == 0)
                    {
                        ascendingFifth = harmonicNumber * 3 / 2;
                    }
                    else
                    {
                        // We look for the next fifth
                        ascendingFifth = harmonicNumber * 3;
                    }

                    int ascendingFifthPackedFormat = ascendingFifth * 2;
                    if (ascendingFifthPackedFormat < tableSizeAsInt)
                    {
                        table[static_cast<size_t> (ascendingFifthPackedFormat)] = power / static_cast<float> (numChannels);
                    }
                }
            }
            this->applyPanAndGainAndRenderToBuffer (audioBuffer, table);
        }
    };
}
