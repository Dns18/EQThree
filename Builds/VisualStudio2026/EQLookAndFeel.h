/*
    EQLookAndFeel.h
    Szinek es a forgatogombok sajat kinezete (sotet, EQ Eight-hez hasonlo stilus).
*/

#pragma once

#include <JuceHeader.h>

namespace EQColours
{
    inline const juce::Colour background{ 0xff1e1e1e };
    inline const juce::Colour panel{ 0xff2a2a2a };
    inline const juce::Colour display{ 0xff151515 };
    inline const juce::Colour grid{ 0xff2f2f2f };
    inline const juce::Colour gridText{ 0xff7a7a7a };
    inline const juce::Colour curve{ 0xffe8e8e8 };
    inline const juce::Colour text{ 0xffd0d0d0 };

    /** Savonkent egy szin: a pont, a gombok es a kijelolt gorbe is ezt kapja. */
    inline juce::Colour band(int index)
    {
        static const juce::Colour colours[] = { juce::Colour(0xfff2a33a),    // narancs
                                                juce::Colour(0xff5bc0eb),    // kek
                                                juce::Colour(0xff9bc53d) };  // zold
        return colours[juce::jlimit(0, 2, index)];
    }
}

//==============================================================================
class EQLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EQLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, EQColours::background);
        setColour(juce::Slider::textBoxTextColourId, EQColours::text);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::rotarySliderFillColourId, EQColours::band(0));
        setColour(juce::ComboBox::backgroundColourId, EQColours::panel.brighter(0.08f));
        setColour(juce::ComboBox::outlineColourId, EQColours::grid);
        setColour(juce::ComboBox::textColourId, EQColours::text);
        setColour(juce::ComboBox::arrowColourId, EQColours::gridText);
        setColour(juce::PopupMenu::backgroundColourId, EQColours::panel);
        setColour(juce::PopupMenu::textColourId, EQColours::text);
        setColour(juce::Label::textColourId, EQColours::text);
        setColour(juce::TextButton::buttonColourId, EQColours::panel.brighter(0.12f));
        setColour(juce::TextButton::textColourOffId, EQColours::gridText);
        setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    }

    /** Forgatogomb: szurke savon szines ertekiv. Szimmetrikus tartomanynal
        (pl. gain: -24..+24 dB) az iv kozeprol indul. */
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float startAngle, float endAngle,
        juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto lineW = juce::jmax(2.0f, radius * 0.14f);
        const auto arcR = radius - lineW * 0.5f;
        const auto angle = startAngle + sliderPos * (endAngle - startAngle);

        auto fill = slider.findColour(juce::Slider::rotarySliderFillColourId);
        if (!slider.isEnabled())
            fill = fill.withAlpha(0.25f);

        const juce::PathStrokeType stroke(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour(EQColours::grid.brighter(0.15f));
        g.strokePath(track, stroke);

        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

        if (std::abs(angle - from) > 0.001f)
        {
            juce::Path value;
            value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                juce::jmin(from, angle), juce::jmax(from, angle), true);
            g.setColour(fill);
            g.strokePath(value, stroke);
        }

        // mutato
        const auto inner = arcR - lineW * 1.6f;
        const juce::Point<float> tip(centre.x + inner * std::sin(angle),
            centre.y - inner * std::cos(angle));
        g.setColour(slider.isEnabled() ? EQColours::text : EQColours::gridText);
        g.drawLine({ centre, tip }, 2.0f);
    }
};
