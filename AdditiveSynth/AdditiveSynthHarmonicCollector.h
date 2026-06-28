/*
  ==============================================================================

    AdditiveSynthHarmonicCollector.h
    Created: 21/4/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "juce_audio_basics/juce_audio_basics.h"
#include <string>
namespace tmf
{
    namespace BaseParameterIdSuffixes
    {
        inline static const std::string level = "_LEVEL";
        inline static const std::string order = "_ORDER";
        inline static const std::string pan = "_PAN";
    }

    struct AdditiveSynthHarmonicCollectorParams
    {
        float level = 0.5f;
        float pan = 0.0f;
        int order = -1;
    };

    class AdditiveSynthHarmonicCollector
    {
    public:
        AdditiveSynthHarmonicCollector() = default;

        virtual ~AdditiveSynthHarmonicCollector() = default;

        void setId (std::string newId) { id = newId; }

        std::string getId() { return id; }

        int getOrder() const { return order; }

        /// <summary>
        /// Adds this collector's harmonics to the wavetable frequency-domain buffer.
        /// </summary>
        /// <param name="audioBuffer">
        /// Input/output buffer containing one packed real-FFT spectrum per channel. The layout is
        /// the even-length IPP Perm format, not Pack or CCS:
        ///   index 0: DC real component
        ///   index 1: Nyquist real component, reserved by this synth and normally left unused
        ///   index 2 * harmonicNumber: real component for harmonicNumber, starting at 1
        ///   index 2 * harmonicNumber + 1: imaginary component for harmonicNumber, starting at 1
        /// Existing contents may have been written by earlier collectors, so collectors can inspect
        /// and add to the buffer. Use applyPanAndGainAndRenderToBuffer() when rendering a temporary
        /// packed table so level and pan are applied consistently.
        /// </param>
        /// <param name="dataSize">
        /// Number of valid packed-float entries collectors may read or write. This can be smaller
        /// than audioBuffer.getNumSamples() because it is capped to the current note's Nyquist
        /// harmonic. Never access an index greater than or equal to dataSize.
        /// </param>
        virtual void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, size_t dataSize) = 0;

        virtual bool waveTableRefreshNeeded() const { return level.isSmoothing() || pan.isSmoothing(); }

        void setParams (AdditiveSynthHarmonicCollectorParams params)
        {
            paramsValues = params;
            doUpdateParameters();
        }

        void prepareToPlay (double sampleRate_, int numOutputChannels)
        {
            this->sampleRate = static_cast<float> (sampleRate_);
            this->numChannels = numOutputChannels;
            level.reset (sampleRate, 0.01f);
            pan.reset (sampleRate, 0.01f);
        }

        void advanceSamples (int numSamples)
        {
            level.skip (numSamples);
            pan.skip (numSamples);
        }

        virtual void setCurrentNote (int note) { currentNote = note; }

        virtual bool updateModTargetValue (juce::String paramid, float value)
        {
            jassert (id != "");
            if (!paramid.startsWith (id))
            {
                return false; // Not a parameter of this collector
            }

            // Optimized for: if (paramid == (String) (id + BaseParameterIdSuffixes::level))
            if (paramid.endsWith (BaseParameterIdSuffixes::level))
            {
                modValues.level = value;
            }
            else if (paramid.endsWith (BaseParameterIdSuffixes::order))
            {
                // We want to cover the whole range, so -(1000 - (-1)) to 1000 - (-1)
                modValues.order = static_cast<int> (juce::jmap (value, -1.0f, 1.0f, -1001.0f, 1001.0f));
            }
            else if (paramid.endsWith (BaseParameterIdSuffixes::pan))
            {
                // We want to cover the whole range, so -(100 - (-100)) to 100 - (-100)
                modValues.pan = juce::jmap (value, -1.0f, 1.0f, -200.0f, 200.0f);
            }

            doUpdateParameters();
            return true;
        }

    protected:
        void doUpdateParameters()
        {
            float limitedLevel = juce::jlimit (0.0f, 1.0f, modValues.level + paramsValues.level);
            float limitedPan = juce::jlimit (-100.0f, 100.0f, modValues.pan + paramsValues.pan);

            if (currentNote == -1)
            {
                this->level.setCurrentAndTargetValue (limitedLevel);
                this->pan.setCurrentAndTargetValue (limitedPan);
            }
            else
            {
                this->level.setTargetValue (limitedLevel);
                this->pan.setTargetValue (limitedPan);
            }

            auto limitedOrder = juce::jlimit (-1, 1000, modValues.order + paramsValues.order);
            this->order = limitedOrder;
        }

        void applyPanAndGainAndRenderToBuffer (juce::AudioBuffer<float>& audioBlock, std::vector<float> data)
        {
            jassert (audioBlock.getNumChannels() >= 2);

            size_t tableSize = data.size();
            jassert (tableSize <= static_cast<size_t> (audioBlock.getNumSamples()));

            juce::FloatVectorOperations::multiply (data.data(), level.getCurrentValue(), static_cast<int> (tableSize));

            // From [-100, 100] to [0, 1]
            auto normalisedPan = (pan.getCurrentValue() * 0.005f) + 0.5f;

            auto leftValue = (1.0f - normalisedPan) * 2.0f;
            auto rightValue = normalisedPan * 2.0f;

            auto auxVector = std::vector<float> (tableSize);
            juce::FloatVectorOperations::multiply (auxVector.data(), data.data(), leftValue, static_cast<int> (tableSize));
            juce::FloatVectorOperations::add (audioBlock.getWritePointer (0), auxVector.data(), static_cast<int> (tableSize));

            juce::FloatVectorOperations::multiply (auxVector.data(), data.data(), rightValue, static_cast<int> (tableSize));
            juce::FloatVectorOperations::add (audioBlock.getWritePointer (1), auxVector.data(), static_cast<int> (tableSize));
        }

        juce::SmoothedValue<float> level = 0.5f;
        juce::SmoothedValue<float> pan = 0.0f;
        int order = -1;

        AdditiveSynthHarmonicCollectorParams paramsValues;
        AdditiveSynthHarmonicCollectorParams modValues { 0.0f, 0.0f, 0 };

        float sampleRate = 0.0f;
        int numChannels = 0;
        int currentNote = -1;

        std::string id = "";
    };
}
