/*
    PluginEditor.h
    Egyszeru, mukodo GUI: savonkent egy oszlop csuszkakkal.
    A frekvenciamenet-gorbe es a spektrumanalizator kesobb kerul ide.
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/** Egy sav vezerloi: be/ki, tipus, Freq, Gain, Q. */
class BandPanel : public juce::Component
{
public:
    BandPanel (EQ3AudioProcessor& p, int bandIndex);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    juce::String title;

    juce::ToggleButton  onButton;
    juce::ComboBox      typeBox;
    juce::Slider        freqSlider, gainSlider, qSlider;
    juce::Label         freqLabel, gainLabel, qLabel;

    std::unique_ptr<APVTS::ButtonAttachment>   onAttachment;
    std::unique_ptr<APVTS::ComboBoxAttachment> typeAttachment;
    std::unique_ptr<APVTS::SliderAttachment>   freqAttachment, gainAttachment, qAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};

//==============================================================================
class EQ3AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQ3AudioProcessorEditor (EQ3AudioProcessor&);
    ~EQ3AudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQ3AudioProcessor& processorRef;

    juce::OwnedArray<BandPanel> bandPanels;

    juce::Slider outputSlider;
    juce::Label  outputLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ3AudioProcessorEditor)
};
