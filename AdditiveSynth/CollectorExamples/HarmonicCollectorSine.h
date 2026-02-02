/*
  ==============================================================================

    HarmonicCollectorSine.h
    Created: 21/4/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "../AdditiveSynthHarmonicCollector.h"
namespace tmf
{
    class HarmonicCollectorSine : public AdditiveSynthHarmonicCollector
    {
    public:
        void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, size_t tableSize) override
        {
            jassert (sampleRate > 0);
            jassert (numChannels > 0);
            std::vector<float> table = std::vector<float> (tableSize, 0);
            if (tableSize >= 2)
            {
                table[2] = 1;
            }
            this->applyPanAndGainAndRenderToBuffer (audioBuffer, table);
        }
    };
}
