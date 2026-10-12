/*
    PluginEditor.h
    EQ Eight-hez hasonlo felulet:
      - felul a frekvenciamenet-kijelzo huzhato savpontokkal (EQDisplay)
      - alul balra savonkent egy sav: be/ki gomb + szurotipus (BandStrip)
      - alul kozepen a kijelolt sav Freq / Gain / Q gombjai
      - alul jobbra a kimeneti hangero
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EQDisplay.h"
#include "EQLookAndFeel.h"

//==============================================================================
/** Egy sav valaszto-csempeje: szamozott be/ki gomb es szurotipus.
    Barhova kattintva rajta kijeloli a savot. */
class BandStrip : public juce::Component
{
public:
    BandStrip(EQ3AudioProcessor& p, int bandIndex);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    void setSelected(bool shouldBeSelected);
    std::function<void()> onSelect;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    int  index;
    bool selected = false;

    juce::TextButton onButton;
    juce::ComboBox   typeBox;

    std::unique_ptr<APVTS::ButtonAttachment>   onAttachment;
    std::unique_ptr<APVTS::ComboBoxAttachment> typeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandStrip)
};

//==============================================================================
class EQ3AudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit EQ3AudioProcessorEditor(EQ3AudioProcessor&);
    ~EQ3AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void selectBand(int index);
    void timerCallback() override;

    EQ3AudioProcessor& processorRef;

    EQLookAndFeel lookAndFeel;      // elol kell allnia: az elemek elott jon letre, utanuk szunik meg
    EQDisplay     display;

    juce::OwnedArray<BandStrip> strips;

    // a kijelolt sav gombjai
    juce::Label  selectedLabel;
    juce::Slider freqKnob, gainKnob, qKnob;
    juce::Label  freqLabel, gainLabel, qLabel;
    std::unique_ptr<APVTS::SliderAttachment> freqAttachment, gainAttachment, qAttachment;

    // kimeneti hangero
    juce::Slider outKnob;
    juce::Label  outLabel;
    std::unique_ptr<APVTS::SliderAttachment> outAttachment;

    juce::Rectangle<int> bottomPanel;
    int selectedBand = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQ3AudioProcessorEditor)
};
