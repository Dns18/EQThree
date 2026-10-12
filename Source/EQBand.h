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

    // Csak ezeknel a tipusoknal van ertelme az erositesnek (gain).
    static bool typeHasGain(Type t)
    {
        return t == Type::LowShelf || t == Type::Bell || t == Type::HighShelf;
    }

    //==============================================================================
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        filter.prepare(spec);
        forceUpdate = true;
        reset();
    }

    void reset()
    {
        filter.reset();
    }

    void setParameters(Type newType, float newFreqHz, float newGainDb, float newQ, bool newEnabled)
    {
        const bool changed = forceUpdate
            || newType != type
            || newFreqHz != freqHz
            || newGainDb != gainDb
            || newQ != q;

        enabled = newEnabled;
        type = newType;
        freqHz = newFreqHz;
        gainDb = newGainDb;
        q = newQ;

        if (changed)
        {
            *filter.state = makeArrayCoefficients(sampleRate, type, freqHz, gainDb, q);
            forceUpdate = false;
        }
    }

    void process(juce::dsp::AudioBlock<float>& block)
    {
        if (!enabled)
            return;

        juce::dsp::ProcessContextReplacing<float> context(block);
        filter.process(context);
    }

    //==============================================================================
    /** Az egyutthato-szamitas objektumkent (heap-en). Csak a GUI hasznalja
        (frekvenciamenet-gorbe), az audio szal soha. */
    static juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients(double sampleRate,
        Type   type,
        float  freqHz,
        float  gainDb,
        float  q)
    {
        return new juce::dsp::IIR::Coefficients<float>(makeArrayCoefficients(sampleRate, type, freqHz, gainDb, q));
    }

    /** Foglalasmentes egyutthato-szamitas az audio szalhoz (std::array, nincs heap). */
    static std::array<float, 6> makeArrayCoefficients(double sampleRate, Type type,
        float freqHz, float gainDb, float q)
    {
        using AC = juce::dsp::IIR::ArrayCoefficients<float>;

        const auto nyquist = (float)(sampleRate * 0.5);
        const auto freq = juce::jlimit(10.0f, nyquist * 0.99f, freqHz);
        const auto quality = juce::jlimit(0.05f, 30.0f, q);
        const auto linGain = juce::Decibels::decibelsToGain(gainDb);

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
    Type   type = Type::Bell;
    float  freqHz = 1000.0f;
    float  gainDb = 0.0f;
    float  q = 0.707f;
    bool   enabled = true;
    bool   forceUpdate = true;
};
