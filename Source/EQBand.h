/*
    EQBand.h
    Egy EQ sav: tipus + frekvencia + gain + Q, biquad (IIR) megvalositassal.

    A tenyleges szurest a juce::dsp::ProcessorDuplicator vegzi, ami csatornankent
    kulon szurot tart fenn, de kozos egyutthato-keszlettel dolgozik.
*/

#pragma once

#include <JuceHeader.h>

class EQBand
{
public:
    enum class Type
    {
        HighPass = 0,
        LowShelf,
        Bell,
        Notch,
        HighShelf,
        LowPass
    };

    // A GUI comboboxok es a parameter-layout is ezt hasznalja, hogy ne csuszhasson el.
    static juce::StringArray getTypeNames()
    {
        return { "High Pass", "Low Shelf", "Bell", "Notch", "High Shelf", "Low Pass" };
    }

    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
		filter.state = new juce::dsp::IIR::Coefficients<float> (1.0f,0.0f,0.0f,1.0f,0.0f,0.0f);
        filter.prepare (spec);
        forceUpdate = true;
        reset();
    }

    void reset()
    {
        filter.reset();
    }

    /** Beallitja a sav parametereit. Az egyutthatokat csak akkor szamolja ujra,
        ha tenylegesen valtozott valami - ez blokkonkent sok felesleges munkat sporol meg. */
    void setParameters (Type newType, float newFreqHz, float newGainDb, float newQ, bool newEnabled)
    {
        const bool changed = forceUpdate
                          || newType    != type
                          || newFreqHz  != freqHz
                          || newGainDb  != gainDb
                          || newQ       != q;

        enabled = newEnabled;
        type    = newType;
        freqHz  = newFreqHz;
        gainDb  = newGainDb;
        q       = newQ;

        if (changed)
        {
            *filter.state = makeArrayCoefficients (sampleRate, type, freqHz, gainDb, q);
            forceUpdate = false;
        }
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! enabled)
            return;

        juce::dsp::ProcessContextReplacing<float> context (block);
        filter.process (context);
    }

    static std::array<float, 6> makeArrayCoefficients(double sampleRate, Type type,
        float freqHz, float gainDb, float q)
    {
        using AC = juce::dsp::IIR::ArrayCoefficients<float>;
        const auto nyquist = sampleRate * 0.5;
        const auto freq = juce::jlimit(10.0, nyquist * 0.99, (double)freqHz);
        const auto quality = juce::jlimit(0.05, 30.0, (double)q);
        const auto linGain = (float)juce::Decibels::decibelsToGain((double)gainDb);

        switch (type)
        {
        case Type::HighPass:  return AC::makeHighPass(sampleRate, freq, quality);
        case Type::LowShelf:  return AC::makeLowShelf(sampleRate, freq, quality, linGain);
        case Type::Bell:      return AC::makePeakFilter(sampleRate, freq, quality, linGain);
        case Type::Notch:     return AC::makeNotch(sampleRate, freq, quality);
        case Type::HighShelf: return AC::makeHighShelf(sampleRate, freq, quality, linGain);
        case Type::LowPass:   return AC::makeLowPass(sampleRate, freq, quality);
        }
        return AC::makeAllPass(sampleRate, freq, quality);
    }

private:
    using Duplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                      juce::dsp::IIR::Coefficients<float>>;

    Duplicator filter;

    double sampleRate = 44100.0;
    Type   type       = Type::Bell;
    float  freqHz     = 1000.0f;
    float  gainDb     = 0.0f;
    float  q          = 0.707f;
    bool   enabled    = true;
    bool   forceUpdate = true;
};
