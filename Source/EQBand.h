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
            filter.state = makeCoefficients (sampleRate, type, freqHz, gainDb, q);
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

    //==============================================================================
    /** Az egyutthato-szamitas. Statikus, hogy a GUI (frekvenciamenet-gorbe) is
        hasznalhassa anelkul, hogy az audio szal allapotahoz nyulna. */
    static juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients (double sampleRate,
                                                                      Type   type,
                                                                      float  freqHz,
                                                                      float  gainDb,
                                                                      float  q)
    {
        // A bilinearis transzformacio miatt a frekvenciat a Nyquist ala kell szoritani.
        const auto nyquist = sampleRate * 0.5;
        const auto freq    = juce::jlimit (10.0, nyquist * 0.99, (double) freqHz);
        const auto quality = juce::jlimit (0.05, 30.0, (double) q);
        const auto linGain = juce::Decibels::decibelsToGain ((double) gainDb);

        switch (type)
        {
            case Type::HighPass:
                return juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, freq, quality);

            case Type::LowShelf:
                return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, freq, quality, (float) linGain);

            case Type::Bell:
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, freq, quality, (float) linGain);

            case Type::Notch:
                return juce::dsp::IIR::Coefficients<float>::makeNotch (sampleRate, freq, quality);

            case Type::HighShelf:
                return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, freq, quality, (float) linGain);

            case Type::LowPass:
                return juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, freq, quality);
        }

        return juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, freq, quality);
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
