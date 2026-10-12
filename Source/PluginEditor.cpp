/*
    PluginEditor.cpp
*/

#include "PluginEditor.h"
#include "EQBand.h"

//==============================================================================
BandStrip::BandStrip(EQ3AudioProcessor& p, int bandIndex)
    : index(bandIndex)
{
    auto& apvts = p.apvts;
    const auto colour = EQColours::band(index);

    // szamozott be/ki gomb a sav szinevel
    onButton.setButtonText(juce::String(index + 1));
    onButton.setClickingTogglesState(true);
    onButton.setColour(juce::TextButton::buttonOnColourId, colour);
    addAndMakeVisible(onButton);
    onAttachment = std::make_unique<APVTS::ButtonAttachment>(
        apvts, EQ3AudioProcessor::bandParamID(index, "on"), onButton);

    typeBox.addItemList(EQBand::getTypeNames(), 1);
    addAndMakeVisible(typeBox);
    typeAttachment = std::make_unique<APVTS::ComboBoxAttachment>(
        apvts, EQ3AudioProcessor::bandParamID(index, "type"), typeBox);

    // a gyerekelemekre kattintas is kijeloli a savot
    onButton.addMouseListener(this, false);
    typeBox.addMouseListener(this, false);
}

void BandStrip::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(selected ? EQColours::panel.brighter(0.12f) : EQColours::panel);
    g.fillRoundedRectangle(bounds, 6.0f);

    if (selected)
    {
        g.setColour(EQColours::band(index));
        g.drawRoundedRectangle(bounds, 6.0f, 1.5f);
    }
}

void BandStrip::resized()
{
    auto area = getLocalBounds().reduced(10);
    onButton.setBounds(area.removeFromTop(28).withSizeKeepingCentre(40, 28));
    area.removeFromTop(8);
    typeBox.setBounds(area.removeFromTop(24));
}

void BandStrip::mouseDown(const juce::MouseEvent&)
{
    if (onSelect != nullptr)
        onSelect();
}

void BandStrip::setSelected(bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

//==============================================================================
static juce::String formatFreq(double hz)
{
    return hz < 1000.0 ? juce::String(juce::roundToInt(hz)) + " Hz"
        : juce::String(hz / 1000.0, 2) + " kHz";
}

static juce::String formatDb(double db)
{
    return juce::String(db, 1) + " dB";
}

//==============================================================================
EQ3AudioProcessorEditor::EQ3AudioProcessorEditor(EQ3AudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), display(p)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(display);
    display.onBandSelected = [this](int band) { selectBand(band); };

    for (int i = 0; i < EQ3AudioProcessor::numBands; ++i)
    {
        auto* strip = strips.add(new BandStrip(p, i));
        strip->onSelect = [this, i] { selectBand(i); };
        addAndMakeVisible(strip);
    }

    auto setUpKnob = [this](juce::Slider& s, juce::Label& l, const juce::String& name)
        {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 18);
            addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setColour(juce::Label::textColourId, EQColours::gridText);
            addAndMakeVisible(l);
        };

    setUpKnob(freqKnob, freqLabel, "Freq");
    setUpKnob(gainKnob, gainLabel, "Gain");
    setUpKnob(qKnob, qLabel, "Q");
    setUpKnob(outKnob, outLabel, "Output");

    outKnob.setColour(juce::Slider::rotarySliderFillColourId, EQColours::curve);
    outAttachment = std::make_unique<APVTS::SliderAttachment>(p.apvts, "outGain", outKnob);
    outKnob.textFromValueFunction = formatDb;
    outKnob.updateText();

    selectedLabel.setJustificationType(juce::Justification::centred);
    selectedLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(selectedLabel);

    selectBand(0);
    startTimerHz(10);

    setResizable(true, true);
    setResizeLimits(620, 440, 1400, 900);
    setSize(780, 500);
}

EQ3AudioProcessorEditor::~EQ3AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void EQ3AudioProcessorEditor::selectBand(int index)
{
    selectedBand = index;
    display.setSelectedBand(index);

    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setSelected(i == index);

    // A harom gombot atkotjuk a kijelolt sav parametereire.
    // Elobb a regi kapcsolatot bontjuk, utana jon az uj.
    freqAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();

    auto& apvts = processorRef.apvts;
    freqAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, EQ3AudioProcessor::bandParamID(index, "freq"), freqKnob);
    gainAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, EQ3AudioProcessor::bandParamID(index, "gain"), gainKnob);
    qAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, EQ3AudioProcessor::bandParamID(index, "q"), qKnob);

    // olvashato kijelzes: "120 Hz", "1.25 kHz", "-7.0 dB", "0.71"
    freqKnob.textFromValueFunction = formatFreq;
    gainKnob.textFromValueFunction = formatDb;
    qKnob.textFromValueFunction = [](double v) { return juce::String(v, 2); };

    const auto colour = EQColours::band(index);
    for (auto* knob : { &freqKnob, &gainKnob, &qKnob })
    {
        knob->setColour(juce::Slider::rotarySliderFillColourId, colour);
        knob->updateText();
        knob->repaint();
    }

    selectedLabel.setText("BAND " + juce::String(index + 1), juce::dontSendNotification);
    selectedLabel.setColour(juce::Label::textColourId, colour);

    timerCallback();   // a gain-gomb engedelyezeset azonnal frissitjuk
}

void EQ3AudioProcessorEditor::timerCallback()
{
    // High Pass / Low Pass / Notch eseten a gain-nek nincs hatasa -> szurkites
    const auto type = (EQBand::Type)(int)processorRef.apvts
        .getRawParameterValue(EQ3AudioProcessor::bandParamID(selectedBand, "type"))->load();
    const auto hasGain = EQBand::typeHasGain(type);

    if (gainKnob.isEnabled() != hasGain)
    {
        gainKnob.setEnabled(hasGain);
        gainLabel.setEnabled(hasGain);
    }
}

//==============================================================================
void EQ3AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(EQColours::background);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("EQ THREE", getLocalBounds().removeFromTop(34).reduced(14, 0),
        juce::Justification::centredLeft);

    g.setColour(EQColours::panel.darker(0.2f));
    g.fillRoundedRectangle(bottomPanel.toFloat(), 6.0f);
}

void EQ3AudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(34);
    area.reduce(10, 0);
    area.removeFromBottom(10);

    // alul a vezerlo panel, felette a kijelzo
    bottomPanel = area.removeFromBottom(150);
    area.removeFromBottom(8);
    display.setBounds(area);

    auto panel = bottomPanel.reduced(8);

    // balra: savvalasztok
    auto stripArea = panel.removeFromLeft(3 * 120);
    for (auto* strip : strips)
        strip->setBounds(stripArea.removeFromLeft(120).reduced(3));

    panel.removeFromLeft(12);

    // jobbra: kimeneti hangero
    auto outArea = panel.removeFromRight(96);
    outLabel.setBounds(outArea.removeFromTop(18));
    outKnob.setBounds(outArea);

    panel.removeFromRight(12);

    // kozepen: a kijelolt sav harom gombja
    selectedLabel.setBounds(panel.removeFromTop(18));
    const auto knobWidth = panel.getWidth() / 3;

    auto layOutKnob = [&](juce::Slider& s, juce::Label& l)
        {
            auto column = panel.removeFromLeft(knobWidth);
            l.setBounds(column.removeFromTop(16));
            s.setBounds(column);
        };

    layOutKnob(freqKnob, freqLabel);
    layOutKnob(gainKnob, gainLabel);
    layOutKnob(qKnob, qLabel);
}
