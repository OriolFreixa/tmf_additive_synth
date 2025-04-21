#pragma once

#include <juce_core/juce_core.h>
#include "FFT.h"
#include "tmf_intercept_synth/tmf_intercept_synth.h"

#include "AdditiveSynth/VoiceInterceptorAdditiveSynth.h"
#include "AdditiveSynth/VoiceInterceptorAdditiveSynthManager.h"
#include "AdditiveSynth/AdditiveSynthHarmonicCollector.h"

// Examples
#include "AdditiveSynth/CollectorExamples/HarmonicCollectorSine.h"

/*
BEGIN_JUCE_MODULE_DECLARATION
    ID:               tmf_additive_synth
    vendor:           tmf
    version:          0.1.0
    name:             Additive Synth
    description:      Additive Synth
    website:          ---
    dependencies:     juce_core, tmf_intercept_synth
END_JUCE_MODULE_DECLARATION 
*/
//==============================================================================