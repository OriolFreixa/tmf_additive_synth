/*
  ==============================================================================

    HarmonicCollectorUnevenBands.h
    Created: 28/6/2026
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once
#include "../AdditiveSynthHarmonicCollectorManager.h"

namespace tmf
{
    class HarmonicCollectorUnevenBands : public AdditiveSynthHarmonicCollector
    {
    public:
        static std::string getDisplayNameStatic()
        {
            return "Noise Band";
        }

        HarmonicCollectorUnevenBands (int newBandIndex = 0, int newFirstHarmonic = 1, int newLastHarmonic = 1)
            : bandIndex (juce::jmax (0, newBandIndex)),
              firstHarmonic (juce::jmax (1, newFirstHarmonic)),
              lastHarmonic (juce::jmax (firstHarmonic, newLastHarmonic))
        {
        }

        void collectHarmonics (juce::AudioBuffer<float>& audioBuffer, size_t tableSize) override
        {
            jassert (sampleRate > 0);
            jassert (numChannels > 0);

            if (tableSize <= 2)
                return;

            std::vector<float> table (tableSize, 0.0f);
            const auto highestHarmonic = static_cast<size_t> ((tableSize - 2u) / 2u);

            for (size_t harmonic = 1; harmonic <= highestHarmonic; ++harmonic)
            {
                if (! isHarmonicInBand (static_cast<int> (harmonic)))
                    continue;

                const auto realIndex = harmonic * 2u;
                table[realIndex] = 1.0f;
            }

            applyPanAndGainAndRenderToBuffer (audioBuffer, table);
        }

        int getBandIndex() const
        {
            return bandIndex;
        }

    private:
        bool isHarmonicInBand (int harmonic) const
        {
            return harmonic >= firstHarmonic
                   && harmonic <= lastHarmonic
                   && (harmonic % 2) == 1;
        }

        int bandIndex = 0;
        int firstHarmonic = 1;
        int lastHarmonic = 1;
    };

    class HarmonicCollectorNoiseManager : public HarmonicCollectorManager<HarmonicCollectorUnevenBands>
    {
    public:
        HarmonicCollectorNoiseManager (int newBandIndex = 0, int newFirstHarmonic = 1, int newLastHarmonic = 1)
            : bandIndex (juce::jmax (0, newBandIndex)),
              firstHarmonic (juce::jmax (1, newFirstHarmonic)),
              lastHarmonic (juce::jmax (firstHarmonic, newLastHarmonic))
        {
            params.level = 0.0f;
            params.order = 1000;
            params.pan = 0.0f;
        }

        string getId() const override
        {
            return getTypeId() + std::to_string (bandIndex);
        }

        vector<string> getAudioParametersIds() override
        {
            return getAudioParametersIdsStatic (bandIndex);
        }

        unique_ptr<juce::AudioProcessorParameterGroup> getAudioParameters() override
        {
            auto id = getId();
            auto displayName = getDisplayName();
            auto result = make_unique<juce::AudioProcessorParameterGroup> (id, displayName, "_");

            result->addChild (make_unique<juce::AudioParameterFloat> (juce::ParameterID { id + BaseParameterIdSuffixes::level, 1 }, displayName + " Level", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f, 0.65f }, params.level));
            result->addChild (make_unique<juce::AudioParameterInt> (juce::ParameterID { id + BaseParameterIdSuffixes::order, 1 }, displayName + " Order", -1, 1000, params.order));
            result->addChild (make_unique<juce::AudioParameterFloat> (juce::ParameterID { id + BaseParameterIdSuffixes::pan, 1 }, displayName + " Pan", juce::NormalisableRange<float> { -100.0f, 100.0f, 0.001f, 1.0f }, params.pan));

            return result;
        }

        shared_ptr<AdditiveSynthHarmonicCollector> getOrCreateHarmonicCollector (size_t index) override
        {
            if (index >= harmonicCollectors.size())
                harmonicCollectors.resize (index + 1);

            if (! harmonicCollectors[index])
            {
                auto newCollector = make_shared<HarmonicCollectorUnevenBands> (bandIndex, firstHarmonic, lastHarmonic);
                newCollector->setParams (params);
                newCollector->setId (getId());
                harmonicCollectors[index] = newCollector;
            }

            return dynamic_pointer_cast<AdditiveSynthHarmonicCollector> (harmonicCollectors[index]);
        }

        void parameterChanged (const juce::String& parameterID, float newValue) override
        {
            const auto id = getId();

            if (parameterID == (String) (id + BaseParameterIdSuffixes::level))
                params.level = newValue;
            else if (parameterID == (String) (id + BaseParameterIdSuffixes::order))
                params.order = static_cast<int> (newValue);
            else if (parameterID == (String) (id + BaseParameterIdSuffixes::pan))
                params.pan = newValue;

            for (auto& collector : harmonicCollectors)
                collector->setParams (params);
        }

        static string getLevelParameterIdStatic (int bandIndex)
        {
            return getAudioParametersIdsStatic (bandIndex)[0];
        }

        static vector<string> getAudioParametersIdsStatic (int bandIndex)
        {
            const auto id = getTypeId() + std::to_string (juce::jmax (0, bandIndex));
            return {
                id + BaseParameterIdSuffixes::level,
                id + BaseParameterIdSuffixes::order,
                id + BaseParameterIdSuffixes::pan
            };
        }

    private:
        string getDisplayName() const override
        {
            return "Noise Band " + std::to_string (bandIndex + 1);
        }

        static string getTypeId()
        {
            return "Noise";
        }

        int bandIndex = 0;
        int firstHarmonic = 1;
        int lastHarmonic = 1;
    };
}
