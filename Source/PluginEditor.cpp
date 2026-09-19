/*
    PluginEditor.cpp
*/

#include "PluginEditor.h"
#include "EQBand.h"

//==============================================================================
BandPanel::BandPanel (EQ3AudioProcessor& p, int bandIndex)
{
    title = "BAND " + juce::String (bandIndex + 1);

    auto& apvts = p.apvts;

    addAndMakeVisible (onButton);
    onAttachment = std::make_unique<APVTS::ButtonAttachment> (
        apvts, EQ3AudioProcessor::bandParamID (bandIndex, "on"), onButton);

    typeBox.addItemList (EQBand::getTypeNames(), 1);
    addAndMakeVisible (typeBox);
    typeAttachment = std::make_unique<APVTS::ComboBoxAttachment> (
        apvts, EQ3AudioProcessor::bandParamID (bandIndex, "type"), typeBox);

    auto setUpSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setUpSlider (freqSlider, freqLabel, "Freq");
    setUpSlider (gainSlider, gainLabel, "Gain");
    setUpSlider (qSlider,    qLabel,    "Q");

    freqSlider.setTextValueSuffix (" Hz");
    gainSlider.setTextValueSuffix (" dB");

    freqAttachment = std::make_unique<APVTS::SliderAttachment> (
        apvts, EQ3AudioProcessor::bandParamID (bandIndex, "freq"), freqSlider);
    gainAttachment = std::make_unique<APVTS::SliderAttachment> (
        apvts, EQ3AudioProcessor::bandParamID (bandIndex, "gain"), gainSlider);
    qAttachment = std::make_unique<APVTS::SliderAttachment> (
        apvts, EQ3AudioProcessor::bandParamID (bandIndex, "q"), qSlider);
}

void BandPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (juce::Colour (0xff2b2b2b));
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (juce::Colour (0xff4a4a4a));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (title, getLocalBounds().removeFromTop (26), juce::Justification::centred);
}

void BandPanel::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto header = area.removeFromTop (22);
    onButton.setBounds (header.removeFromRight (26));

    area.removeFromTop (6);
    typeBox.setBounds (area.removeFromTop (24));
    area.removeFromTop (10);

    const auto knobHeight = area.getHeight() / 3;

    auto layOutKnob = [&] (juce::Slider& s, juce::Label& l)
    {
        auto row = area.removeFromTop (knobHeight);
        l.setBounds (row.removeFromTop (16));
        s.setBounds (row.reduced (4, 0));
    };

    layOutKnob (freqSlider, freqLabel);
    layOutKnob (gainSlider, gainLabel);
    layOutKnob (qSlider,    qLabel);
}

//==============================================================================
EQ3AudioProcessorEditor::EQ3AudioProcessorEditor (EQ3AudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    for (int i = 0; i < EQ3AudioProcessor::numBands; ++i)
        addAndMakeVisible (bandPanels.add (new BandPanel (p, i)));

    outputSlider.setSliderStyle (juce::Slider::LinearVertical);
    outputSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    outputSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (outputSlider);

    outputLabel.setText ("Out", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputLabel);

    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, "outGain", outputSlider);

    setResizable (true, true);
    setResizeLimits (520, 380, 1200, 800);
    setSize (640, 440);
}

void EQ3AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("EQ THREE", getLocalBounds().removeFromTop (34).reduced (14, 0),
                juce::Justification::centredLeft);
}

void EQ3AudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (34);
    area.reduce (10, 10);

    // Jobb oldalt a kimeneti hangero
    auto outArea = area.removeFromRight (70);
    outputLabel.setBounds (outArea.removeFromTop (18));
    outputSlider.setBounds (outArea.reduced (4));

    area.removeFromRight (8);

    // A maradek harom egyenlo oszlop
    const auto panelWidth = area.getWidth() / EQ3AudioProcessor::numBands;

    for (auto* panel : bandPanels)
        panel->setBounds (area.removeFromLeft (panelWidth));
}
