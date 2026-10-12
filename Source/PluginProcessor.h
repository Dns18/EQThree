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
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Parameter ID-k egysegesen: "band0_freq", "band1_gain", stb. */
    static juce::String bandParamID(int bandIndex, juce::StringRef suffix)
    {
        return "band" + juce::String(bandIndex) + "_" + suffix;
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts{ *this, nullptr, "PARAMETERS", createParameterLayout() };

    // A GUI (frekvenciamenet-gorbe) is olvassa, ezert atomi.
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(); }

    /** A spektrumanalizatornak: a GUI innen olvassa ki a legutobb feldolgozott
        (mono) mintakat. Visszaadja, hany mintat masolt a dest-be. */
    int readAnalyserSamples(float* dest, int maxNumSamples);

private:
    void updateBandsFromParameters();
    void pushAnalyserSamples(const juce::AudioBuffer<float>& buffer);

    std::array<EQBand, (size_t)numBands> bands;
    juce::SmoothedValue<float> outputGain{ 1.0f };
    std::atomic<double> currentSampleRate{ 44100.0 };

    // Zarolas nelkuli sor (egy iro: audio szal, egy olvaso: GUI) az analizatorhoz.
    // A tarolo elore le van foglalva, igy az audio szalon nincs memoriafoglalas.
    static constexpr int analyserFifoSize = 8192;
    juce::AbstractFifo analyserFifo{ analyserFifoSize };
    std::array<float, analyserFifoSize> analyserBuffer{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQ3AudioProcessor)
};
