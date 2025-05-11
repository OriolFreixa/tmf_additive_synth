/*
  ==============================================================================

    HarmonicCollectorSine.h
    Created: 11/5/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "../AdditiveSynthHarmonicCollector.h"
namespace tmf
{
    class HarmonicCollectorOctaves : public AdditiveSynthHarmonicCollector
    {
    public:
        void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, int tableSize) override
        {
            jassert (sampleRate > 0);
            jassert (numChannels > 0);
            std::vector<float> table = std::vector<float>(tableSize, 0);
            for (int i = 2; 2*i < tableSize; i *= 2)
            {
                table[2*i] = 1;
            }

            this->applyPanAndGainAndRenderToBuffer (audioBuffer, table);
        }
    };
}