/*
    EQDisplay.h
    A frekvenciamenet-kijelzo (EQ Eight-hez hasonlo):
      - logaritmikus frekvenciatengely (20 Hz - 20 kHz), dB-racs (+-24 dB)
      - az osszesitett frekvenciagorbe + a kijelolt sav sajat gorbeje
      - savonkent egy huzhato pont: vizszintesen frekvencia, fuggolegesen gain
      - gorgo (eger kereke) a ponton: Q, dupla kattintas: sav be/ki
      - a hatterben spektrumanalizator (FFT) mutatja a kimeno hangot

    Csak az APVTS parametereit olvassa es irja; az audio szallal egyedul
    a zarolas nelkuli analizator-soron keresztul erintkezik (readAnalyserSamples).
*/
#include <JuceHeader.h>
#include "PluginProcessor.h"

class EQDisplay : public juce::Component,
    private juce::Timer
{
public:
    explicit EQDisplay(EQ3AudioProcessor& p);
    ~EQDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    /** A szerkeszto (editor) hivja, ha lent a savvalasztoval masik savot jelolnek ki. */
    void setSelectedBand(int index);

    /** Akkor hivodik, ha a kijelzon kattintanak egy savpontra. */
    std::function<void(int)> onBandSelected;

private:
    static constexpr int   numBands = EQ3AudioProcessor::numBands;
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float dbRange = 24.0f;     // a kijelzo +-24 dB-t mutat
    static constexpr float nodeRadius = 9.0f;

    struct BandState
    {
        EQBand::Type type = EQBand::Type::Bell;
        float freq = 0.0f, gain = 0.0f, q = 0.0f;
        bool  on = false;

        bool operator!= (const BandState& o) const
        {
            return type != o.type || freq != o.freq || gain != o.gain || q != o.q || on != o.on;
        }
    };

    void timerCallback() override;
    bool readParameters();                      // true, ha valtozott valami
    void updateCurves();

    // spektrumanalizator
    bool updateSpectrum();                      // true, ha valtozott a spektrum
    void buildSpectrumPath();
    float spectrumDbAt(float freqLow, float freqHigh) const;

    // koordinata-atszamitasok
    juce::Rectangle<float> plotArea() const;
    float xForFreq(float freq) const;
    float freqForX(float x) const;
    float yForDb(float db) const;
    float dbForY(float y) const;
    juce::Point<float> nodePosition(int band) const;
    int hitTestNode(juce::Point<float> pos) const;

    // parameterek irasa (a DAW is ertesul rola -> automatizalhato)
    juce::RangedAudioParameter* getParam(int band, juce::StringRef name) const;
    static void setParamValue(juce::RangedAudioParameter* param, float value);

    EQ3AudioProcessor& processor;

    std::array<BandState, numBands> bands{};
    double sampleRate = 44100.0;

    // --- Spektrumanalizator ---
    static constexpr int   fftOrder = 11;                 // 2^11 = 2048 minta
    static constexpr int   fftSize = 1 << fftOrder;
    static constexpr float specMinDb = -90.0f;             // a spektrum skalaja (dBFS)
    static constexpr float specMaxDb = 0.0f;
    static constexpr float specTiltDb = 3.0f;               // dB/oktav doles 1 kHz korul (a zene magasban halkabb)

    juce::dsp::FFT fft{ fftOrder };
    juce::dsp::WindowingFunction<float> window{ (size_t)fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::array<float, fftSize>     history{};              // a legutobbi 2048 minta
    std::array<float, fftSize * 2> fftData{};              // az FFT munkaterulete
    std::array<float, fftSize>     incoming{};             // ide olvassuk ki a sorbol
    std::array<float, fftSize / 2> spectrumDb{};           // simitott ertekek binenkent
    juce::Path spectrumPath;

    juce::Path totalCurve;
    std::array<juce::Path, numBands> bandCurves;

    int  selectedBand = 0;
    int  hoveredBand = -1;
    int  draggingBand = -1;
    bool draggingGain = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQDisplay)
};