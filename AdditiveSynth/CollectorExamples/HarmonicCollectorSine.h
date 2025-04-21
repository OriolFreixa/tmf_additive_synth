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
        void collectHarmonics (float* data, int tableSize) override
        {
            data[2] = 1.0f;
        }

    };
}