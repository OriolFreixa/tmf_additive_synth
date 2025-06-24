/*
  ==============================================================================

    AdditiveSynthHarmonicCollector.h
    Created: 21/4/2025
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
namespace tmf
{
    namespace BaseParameterIdSuffixes
    {
        inline static const string level = "_LEVEL";
        inline static const string order = "_ORDER";
        inline static const string pan = "_PAN";
    };

    struct AdditiveSynthHarmonicCollectorParams
    {
        float level = 0.5;
        float pan = 0;
        int order = -1;
    };

    class AdditiveSynthHarmonicCollector
    {
    public:
        AdditiveSynthHarmonicCollector() = default;

        virtual ~AdditiveSynthHarmonicCollector() = default;

        void setId (string newId) { id = newId; }

        string getId () { return id; }

        int getOrder() const { return order; }

        /// <summary>
        /// Sets the harmonics of the collector
        /// </summary>
        /// <param name="audioBuffer">Input/output audio buffer. Content in the frequency domain in packed format. Info: https://www.intel.com/content/www/us/en/docs/ipp/developer-guide-reference/2022-0/packed-formats.html</param>
        /// <param name="dataSize">Length of the audioBuffer</param>
        virtual void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, int dataSize) = 0;

        virtual bool waveTableRefreshNeeded() const { return level.isSmoothing() || pan.isSmoothing(); }

        void setParams (AdditiveSynthHarmonicCollectorParams params)
        {
            paramsValues = params;
            doUpdateParameters();
        }

        void prepareToPlay (double sampleRate, int numOutputChannels)
        {
            this->sampleRate = sampleRate;
            this->numChannels = numOutputChannels;
            level.reset(sampleRate, 0.01);
            pan.reset(sampleRate, 0.01);
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
            if (paramid.endsWith(BaseParameterIdSuffixes::level))
            {
                modValues.level = value;
            }
            else if (paramid.endsWith (BaseParameterIdSuffixes::order))
            {
                modValues.order = juce::jmap(value, 0.f, 1000.f);
            }
            else if (paramid.endsWith (BaseParameterIdSuffixes::pan))
            {
                // We want to cover the whole range, so 0 to 100 - (-100)
                modValues.pan = juce::jmap (value, 0.f, 200.f);
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

            float limitedOrder = juce::jlimit (-1, 1000, modValues.order + paramsValues.order);
            this->order = limitedOrder;
        }

        void applyPanAndGainAndRenderToBuffer(juce::AudioBuffer<float>& audioBlock, std::vector<float> data)
        {
            jassert (audioBlock.getNumChannels() == 2);

            int tableSize = data.size();
            juce::FloatVectorOperations::multiply (data.data(), level.getCurrentValue(), tableSize);

            // From [-100, 100] to [0, 1]
            auto normalisedPan = (pan.getCurrentValue() * 0.005) + 0.5;

            auto leftValue = (1.0 - normalisedPan) * 2;
            auto rightValue = normalisedPan * 2;

            auto auxVector = std::vector<float> (tableSize);
            juce::FloatVectorOperations::multiply (auxVector.data(), data.data(), leftValue, tableSize);
            juce::FloatVectorOperations::add (audioBlock.getWritePointer (0), auxVector.data(), tableSize);

            juce::FloatVectorOperations::multiply (auxVector.data(), data.data(), rightValue, tableSize);
            juce::FloatVectorOperations::add (audioBlock.getWritePointer (1), auxVector.data(), tableSize);
        }

        juce::SmoothedValue<float> level = 0.5;
        juce::SmoothedValue<float> pan = 0;
        int order = -1;

        AdditiveSynthHarmonicCollectorParams paramsValues;
        AdditiveSynthHarmonicCollectorParams modValues {0, 0, 0};

        float sampleRate = 0;
        int numChannels = 0;
        int currentNote = -1;

        string id = "";
    };
}