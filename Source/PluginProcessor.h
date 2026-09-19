/*
    PluginProcessor.h
    3 savos EQ - audio feldolgozo osztaly.
*/

#pragma once

#include <JuceHeader.h>
#include "EQBand.h"

class EQ3AudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 3;

    //==============================================================================
    EQ3AudioProcessor();
    ~EQ3AudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return JucePlugin_Name; }
    bool acceptsMidi() const override                        { return false; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Parameter ID-k egysegesen: "band0_freq", "band1_gain", stb. */
    static juce::String bandParamID (int bandIndex, juce::StringRef suffix)
    {
        return "band" + juce::String (bandIndex) + "_" + suffix;
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMETERS", createParameterLayout() };

    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

private:
    void updateBandsFromParameters();

    std::array<EQBand, (size_t) numBands> bands;
    juce::SmoothedValue<float> outputGain { 1.0f };
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ3AudioProcessor)
};
