#include "EQDisplay.h"
#include "EQLookAndFeel.h"

//==============================================================================
EQDisplay::EQDisplay(EQ3AudioProcessor& p)
    : processor(p)
{
    spectrumDb.fill(specMinDb);
    readParameters();
    startTimerHz(30);   // 30-szor masodpercenkent megnezi, valtozott-e valami
}

EQDisplay::~EQDisplay()
{
    stopTimer();
}

//==============================================================================
juce::RangedAudioParameter* EQDisplay::getParam(int band, juce::StringRef name) const
{
    return processor.apvts.getParameter(EQ3AudioProcessor::bandParamID(band, name));
}

void EQDisplay::setParamValue(juce::RangedAudioParameter* param, float value)
{
    if (param != nullptr)
        param->setValueNotifyingHost(param->convertTo0to1(value));
}

bool EQDisplay::readParameters()
{
    bool changed = false;
    auto& apvts = processor.apvts;

    for (int i = 0; i < numBands; ++i)
    {
        BandState s;
        s.type = (EQBand::Type)(int)apvts.getRawParameterValue(EQ3AudioProcessor::bandParamID(i, "type"))->load();
        s.freq = apvts.getRawParameterValue(EQ3AudioProcessor::bandParamID(i, "freq"))->load();
        s.gain = apvts.getRawParameterValue(EQ3AudioProcessor::bandParamID(i, "gain"))->load();
        s.q = apvts.getRawParameterValue(EQ3AudioProcessor::bandParamID(i, "q"))->load();
        s.on = apvts.getRawParameterValue(EQ3AudioProcessor::bandParamID(i, "on"))->load() > 0.5f;

        if (s != bands[(size_t)i])
        {
            bands[(size_t)i] = s;
            changed = true;
        }
    }

    const auto sr = processor.getCurrentSampleRate();

    if (sr > 0.0 && sr != sampleRate)
    {
        sampleRate = sr;
        changed = true;
    }

    return changed;
}

void EQDisplay::timerCallback()
{
    const bool paramsChanged = readParameters();
    if (paramsChanged)
        updateCurves();

    const bool spectrumChanged = updateSpectrum();
    if (spectrumChanged)
        buildSpectrumPath();

    if (paramsChanged || spectrumChanged)
        repaint();
}

//==============================================================================
// Spektrumanalizator: a GUI szalon fut, az audio szal csak a sorba ir.

bool EQDisplay::updateSpectrum()
{
    // 1) Az uj mintak kiolvasasa a sorbol a "history" vegere (a regiek elore csusznak).
    bool gotNewData = false;

    for (;;)
    {
        const auto num = processor.readAnalyserSamples(incoming.data(), (int)incoming.size());
        if (num <= 0)
            break;

        gotNewData = true;

        if (num >= fftSize)
        {
            std::copy(incoming.begin(), incoming.end(), history.begin());
        }
        else
        {
            std::move(history.begin() + num, history.end(), history.begin());
            std::copy(incoming.begin(), incoming.begin() + num, history.end() - num);
        }
    }

    if (!gotNewData)
    {
        // Nincs hang (pl. megallt a lejatszas): a spektrum lassan lecseng.
        bool anyAboveFloor = false;
        for (auto& v : spectrumDb)
        {
            v = juce::jmax(specMinDb, v - 1.5f);
            anyAboveFloor = anyAboveFloor || v > specMinDb;
        }
        return anyAboveFloor;
    }

    // 2) Ablakozas (Hann) es FFT -> binenkenti amplitudo.
    std::copy(history.begin(), history.end(), fftData.begin());
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable(fftData.data(), (size_t)fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    // 3) dB-re valtas es simitas: gyors felfutas, lassu (1,5 dB / kepkocka) lecsenges.
    for (size_t i = 0; i < spectrumDb.size(); ++i)
    {
        const auto db = juce::Decibels::gainToDecibels(fftData[i] * 2.0f / (float)fftSize, specMinDb);
        spectrumDb[i] = juce::jmax(db, spectrumDb[i] - 1.5f);
    }

    return true;
}

float EQDisplay::spectrumDbAt(float freqLow, float freqHigh) const
{
    const auto binWidth = (float)sampleRate / (float)fftSize;
    const auto lastBin = (int)spectrumDb.size() - 1;

    // Melyen egy bin tobb pixelt is lefed: ott ket szomszedos bin kozott
    // linearisan interpolalunk, kulonben lepcsos lenne a rajz.
    if (freqHigh - freqLow < binWidth)
    {
        const auto pos = juce::jlimit(1.0f, (float)lastBin, 0.5f * (freqLow + freqHigh) / binWidth);
        const auto i0 = (int)pos;
        const auto i1 = juce::jmin(i0 + 1, lastBin);
        const auto frac = pos - (float)i0;
        return spectrumDb[(size_t)i0] + frac * (spectrumDb[(size_t)i1] - spectrumDb[(size_t)i0]);
    }

    // Magasan egy pixelre tobb bin jut: ezek kozul a legnagyobbat mutatjuk.
    const auto b0 = juce::jlimit(1, lastBin, (int)std::floor(freqLow / binWidth));
    const auto b1 = juce::jlimit(b0, lastBin, (int)std::ceil(freqHigh / binWidth));

    auto value = specMinDb;
    for (int b = b0; b <= b1; ++b)
        value = juce::jmax(value, spectrumDb[(size_t)b]);

    return value;
}

void EQDisplay::buildSpectrumPath()
{
    spectrumPath.clear();

    const auto area = plotArea();
    if (area.getWidth() < 2.0f)
        return;

    const auto yForSpecDb = [&](float db)
        {
            const auto norm = juce::jlimit(0.0f, 1.0f, (db - specMinDb) / (specMaxDb - specMinDb));
            return area.getBottom() - norm * area.getHeight();
        };

    spectrumPath.startNewSubPath(area.getX(), area.getBottom());

    for (float x = area.getX(); x <= area.getRight(); x += 2.0f)
    {
        const auto f0 = freqForX(x);
        const auto f1 = freqForX(x + 2.0f);
        const auto tilt = specTiltDb * std::log2(f0 / 1000.0f);   // +3 dB/oktav 1 kHz-tol felfele
        const auto db = spectrumDbAt(f0, f1);
        spectrumPath.lineTo(x, yForSpecDb(db > specMinDb ? db + tilt : specMinDb));
    }

    spectrumPath.lineTo(area.getRight(), area.getBottom());
    spectrumPath.closeSubPath();
}

//==============================================================================
// Koordinatak: vizszintesen logaritmikus frekvencia, fuggolegesen linearis dB.

juce::Rectangle<float> EQDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced(8.0f).withTrimmedLeft(26.0f).withTrimmedBottom(16.0f);
}

float EQDisplay::xForFreq(float freq) const
{
    const auto area = plotArea();
    return area.getX() + area.getWidth() * std::log(freq / minFreq) / std::log(maxFreq / minFreq);
}

float EQDisplay::freqForX(float x) const
{
    const auto area = plotArea();
    const auto norm = juce::jlimit(0.0f, 1.0f, (x - area.getX()) / area.getWidth());
    return minFreq * std::pow(maxFreq / minFreq, norm);
}

float EQDisplay::yForDb(float db) const
{
    const auto area = plotArea();
    return area.getCentreY() - db / dbRange * area.getHeight() * 0.5f;
}

float EQDisplay::dbForY(float y) const
{
    const auto area = plotArea();
    return (area.getCentreY() - y) / (area.getHeight() * 0.5f) * dbRange;
}

juce::Point<float> EQDisplay::nodePosition(int band) const
{
    const auto& b = bands[(size_t)band];
    const auto db = EQBand::typeHasGain(b.type) ? b.gain : 0.0f;   // HP/LP/Notch: a 0 dB vonalon
    return { xForFreq(b.freq), yForDb(db) };
}

int EQDisplay::hitTestNode(juce::Point<float> pos) const
{
    // A kijelolt sav elonyt kap, ha ket pont fedi egymast.
    if (nodePosition(selectedBand).getDistanceFrom(pos) <= nodeRadius + 4.0f)
        return selectedBand;

    for (int i = numBands - 1; i >= 0; --i)
        if (nodePosition(i).getDistanceFrom(pos) <= nodeRadius + 4.0f)
            return i;

    return -1;
}

//==============================================================================
void EQDisplay::updateCurves()
{
    totalCurve.clear();
    for (auto& path : bandCurves)
        path.clear();

    const auto area = plotArea();
    if (area.getWidth() < 2.0f)
        return;

    // Ugyanazokkal a fuggvenyekkel szamolunk, mint az audio szal -> a gorbe pontosan azt mutatja, amit hallunk.
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, numBands> coeffs;
    for (size_t i = 0; i < coeffs.size(); ++i)
        coeffs[i] = EQBand::makeCoefficients(sampleRate, bands[i].type, bands[i].freq, bands[i].gain, bands[i].q);

    const auto limitDb = [](double db) { return (float)juce::jlimit(-60.0, 60.0, db); };
    const auto maxPlotHz = sampleRate * 0.5 * 0.999;   // a Nyquist felett nincs ertelmezve
    const int  numPoints = juce::roundToInt(area.getWidth());

    for (int px = 0; px <= numPoints; ++px)
    {
        const auto x = area.getX() + (float)px;
        const auto freq = juce::jmin((double)freqForX(x), maxPlotHz);
        double totalDb = 0.0;

        for (size_t i = 0; i < coeffs.size(); ++i)
        {
            const auto magnitude = coeffs[i]->getMagnitudeForFrequency(freq, sampleRate);
            const auto db = juce::Decibels::gainToDecibels(magnitude, -100.0);

            if (bands[i].on)
                totalDb += db;   // sorba kotott szurok: a dB-ek osszeadodnak

            const auto y = yForDb(limitDb(db));
            if (px == 0) bandCurves[i].startNewSubPath(x, y);
            else         bandCurves[i].lineTo(x, y);
        }

        const auto y = yForDb(limitDb(totalDb));
        if (px == 0) totalCurve.startNewSubPath(x, y);
        else         totalCurve.lineTo(x, y);
    }
}

void EQDisplay::resized()
{
    updateCurves();
    buildSpectrumPath();
}

//==============================================================================
void EQDisplay::paint(juce::Graphics& g)
{
    const auto area = plotArea();

    g.setColour(EQColours::display);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

    // --- Racs: frekvenciak ---
    g.setFont(juce::FontOptions(11.0f));
    const float gridFreqs[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

    for (auto f : gridFreqs)
    {
        const auto x = xForFreq(f);
        g.setColour(EQColours::grid);
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());

        const auto text = f >= 1000.0f ? juce::String(juce::roundToInt(f / 1000.0f)) + "k"
            : juce::String(juce::roundToInt(f));
        g.setColour(EQColours::gridText);
        g.drawText(text, juce::Rectangle<float>(x - 20.0f, area.getBottom() + 2.0f, 40.0f, 14.0f),
            juce::Justification::centred);
    }

    // --- Racs: dB ---
    for (float db = -dbRange; db <= dbRange; db += 6.0f)
    {
        const auto y = yForDb(db);
        g.setColour(db == 0.0f ? EQColours::grid.brighter(0.4f) : EQColours::grid);
        g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());

        const auto text = (db > 0.0f ? "+" : "") + juce::String(juce::roundToInt(db));
        g.setColour(EQColours::gridText);
        g.drawText(text, juce::Rectangle<float>(area.getX() - 30.0f, y - 7.0f, 26.0f, 14.0f),
            juce::Justification::centredRight);
    }

    // --- Gorbek (a terulet szelen levagva) ---
    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(area.toNearestInt());

        // a spektrum halvanyan, minden mas mogott
        g.setColour(EQColours::curve.withAlpha(0.10f));
        g.fillPath(spectrumPath);
        g.setColour(EQColours::curve.withAlpha(0.22f));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));

        const auto colour = EQColours::band(selectedBand);
        const auto& sel = bands[(size_t)selectedBand];

        // a kijelolt sav sajat gorbeje, alatta kitoltes a 0 dB vonalig
        auto fill = bandCurves[(size_t)selectedBand];
        const auto zeroY = yForDb(0.0f);
        fill.lineTo(area.getRight(), zeroY);
        fill.lineTo(area.getX(), zeroY);
        fill.closeSubPath();

        g.setColour(colour.withAlpha(sel.on ? 0.15f : 0.05f));
        g.fillPath(fill);
        g.setColour(colour.withAlpha(sel.on ? 0.7f : 0.25f));
        g.strokePath(bandCurves[(size_t)selectedBand], juce::PathStrokeType(1.5f));

        // az osszesitett gorbe (ezt halljuk)
        g.setColour(EQColours::curve);
        g.strokePath(totalCurve, juce::PathStrokeType(2.0f));
    }

    // --- Savpontok ---
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));

    for (int i = 0; i < numBands; ++i)
    {
        const auto pos = nodePosition(i);
        const bool active = (i == selectedBand || i == hoveredBand || i == draggingBand);
        const auto r = active ? nodeRadius + 1.5f : nodeRadius;
        const auto colour = EQColours::band(i).withAlpha(bands[(size_t)i].on ? 1.0f : 0.35f);
        const auto circle = juce::Rectangle<float>(r * 2.0f, r * 2.0f).withCentre(pos);

        g.setColour(colour);
        g.fillEllipse(circle);

        if (i == selectedBand)
        {
            g.setColour(juce::Colours::white);
            g.drawEllipse(circle.expanded(2.0f), 1.5f);
        }

        g.setColour(juce::Colours::black);
        g.drawText(juce::String(i + 1), circle, juce::Justification::centred);
    }
}

//==============================================================================
// Egerkezeles. A begin/endChangeGesture jelzi a DAW-nak, hogy a felhasznalo
// "fogja" a parametert - igy az automatizalas felvetele is helyesen mukodik.

void EQDisplay::mouseDown(const juce::MouseEvent& e)
{
    draggingBand = hitTestNode(e.position);

    if (draggingBand < 0)
        return;

    setSelectedBand(draggingBand);
    if (onBandSelected != nullptr)
        onBandSelected(draggingBand);

    draggingGain = EQBand::typeHasGain(bands[(size_t)draggingBand].type);

    if (auto* p = getParam(draggingBand, "freq")) p->beginChangeGesture();
    if (draggingGain)
        if (auto* p = getParam(draggingBand, "gain")) p->beginChangeGesture();
}

void EQDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingBand < 0)
        return;

    setParamValue(getParam(draggingBand, "freq"), freqForX(e.position.x));

    if (draggingGain)
        setParamValue(getParam(draggingBand, "gain"),
            juce::jlimit(-dbRange, dbRange, dbForY(e.position.y)));
}

void EQDisplay::mouseUp(const juce::MouseEvent&)
{
    if (draggingBand < 0)
        return;

    if (auto* p = getParam(draggingBand, "freq")) p->endChangeGesture();
    if (draggingGain)
        if (auto* p = getParam(draggingBand, "gain")) p->endChangeGesture();

    draggingBand = -1;
    repaint();
}

void EQDisplay::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto band = hitTestNode(e.position);
    if (band < 0)
        return;

    // dupla kattintas a ponton: sav be/ki
    if (auto* p = getParam(band, "on"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(bands[(size_t)band].on ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
}

void EQDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto hit = hitTestNode(e.position);
    const auto band = hit >= 0 ? hit : selectedBand;

    // gorgo: Q szorzasa, igy kis es nagy Q-ertekeknel is egyforman finom
    if (auto* p = getParam(band, "q"))
    {
        const auto newQ = bands[(size_t)band].q * std::exp(wheel.deltaY * 2.0f);
        p->beginChangeGesture();
        setParamValue(p, newQ);
        p->endChangeGesture();
    }
}

void EQDisplay::mouseMove(const juce::MouseEvent& e)
{
    const auto hit = hitTestNode(e.position);

    if (hit != hoveredBand)
    {
        hoveredBand = hit;
        setMouseCursor(hit >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EQDisplay::mouseExit(const juce::MouseEvent&)
{
    hoveredBand = -1;
    repaint();
}

void EQDisplay::setSelectedBand(int index)
{
    selectedBand = juce::jlimit(0, numBands - 1, index);
    repaint();
}
