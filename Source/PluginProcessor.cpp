/*
    PluginProcessor.cpp
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EQ3AudioProcessor::EQ3AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EQ3AudioProcessor::createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto typeNames = EQBand::getTypeNames();

    // Alapertelmezesek savonkent: low shelf 100 Hz / bell 1 kHz / high shelf 8 kHz
    const int   defaultTypes[numBands] = { 1, 2, 4 };
    const float defaultFreqs[numBands] = { 100.0f, 1000.0f, 8000.0f };

    for (int i = 0; i < numBands; ++i)
    {
        const auto bandName = "Band " + String (i + 1) + " ";

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { bandParamID (i, "on"), 1 },
            bandName + "On",
            true));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { bandParamID (i, "type"), 1 },
            bandName + "Type",
            typeNames,
            defaultTypes[i]));

        // A 0.25-os skew teszi logaritmikussa a frekvencia-csuszkat.
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { bandParamID (i, "freq"), 1 },
            bandName + "Freq",
            NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f),
            defaultFreqs[i],
            AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { bandParamID (i, "gain"), 1 },
            bandName + "Gain",
            NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
            0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { bandParamID (i, "q"), 1 },
            bandName + "Q",
            NormalisableRange<float> (0.1f, 18.0f, 0.0f, 0.3f),
            0.707f));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "outGain", 1 },
        "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

//==============================================================================
void EQ3AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    for (auto& band : bands)
        band.prepare (spec);

    outputGain.reset (sampleRate, 0.02);  // 20 ms-os simitas a zipper noise ellen
    outputGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outGain")->load()));

    updateBandsFromParameters();
}

void EQ3AudioProcessor::releaseResources()
{
    for (auto& band : bands)
        band.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EQ3AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}
#endif

//==============================================================================
void EQ3AudioProcessor::updateBandsFromParameters()
{
    for (int i = 0; i < numBands; ++i)
    {
        const auto type    = (EQBand::Type) (int) apvts.getRawParameterValue (bandParamID (i, "type"))->load();
        const auto freq    = apvts.getRawParameterValue (bandParamID (i, "freq"))->load();
        const auto gain    = apvts.getRawParameterValue (bandParamID (i, "gain"))->load();
        const auto q       = apvts.getRawParameterValue (bandParamID (i, "q"))->load();
        const auto enabled = apvts.getRawParameterValue (bandParamID (i, "on"))->load() > 0.5f;

        bands[(size_t) i].setParameters (type, freq, gain, q, enabled);
    }
}

void EQ3AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Kotelezo: kikapcsolja a denormal szamokat, amik brutalisan lassithatjak az IIR szurot.
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateBandsFromParameters();

    juce::dsp::AudioBlock<float> block (buffer);

    for (auto& band : bands)
        band.process (block);

    outputGain.setTargetValue (
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outGain")->load()));

    buffer.applyGainRamp (0, buffer.getNumSamples(),
                          outputGain.getCurrentValue(),
                          outputGain.skip (buffer.getNumSamples()));
}

//==============================================================================
juce::AudioProcessorEditor* EQ3AudioProcessor::createEditor()
{
    return new EQ3AudioProcessorEditor (*this);
}

//==============================================================================
void EQ3AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream (destData, true);
    apvts.state.writeToStream (stream);
}

void EQ3AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);

    if (tree.isValid())
    {
        apvts.replaceState (tree);
        updateBandsFromParameters();
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQ3AudioProcessor();
}
