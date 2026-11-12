#pragma once
#include <juce_dsp/juce_dsp.h>
#include <pixelui/Widgets.h>
#include "PluginProcessor.h"

/** Iron S's single accent: glowing hot metal. */
namespace ironpal
{
inline constexpr px::RGB heat = 0xf06a1e; // accent
inline constexpr px::RGB hot  = 0xffd08a; // glow highlight
inline constexpr px::RGB dim  = 0x4d2008; // unlit / dim accent
inline constexpr px::RGB deep = 0x1a0c05; // accent-tinted black
}

inline float smooth01 (float a, float b, float x) noexcept
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

//==============================================================================
/** Invisible hit area over the ironing scene: vertical drag moves the iron = threshold (down = press harder). */
class IronHandle : public px::Widget
{
public:
    explicit IronHandle (juce::RangedAudioParameter& p)
        : param (p), attachment (p, [this] (float v) { norm = param.convertTo0to1 (v); })
    {
        attachment.sendInitialUpdate();
    }

    std::function<void()> onPoke;
    float dbPerPixel = 1.5f; // set by the editor
    bool isDragging() const noexcept { return dragging; }

    void paint (px::Canvas&) override {}

    void mouseDown (juce::Point<float> p, const juce::ModifierKeys&) override
    {
        dragging = true; startY = p.y; startNorm = norm;
        attachment.beginGesture();
        if (onPoke) onPoke();
    }

    void mouseDrag (juce::Point<float> p, const juce::ModifierKeys& m) override
    {
        // the iron follows the mouse 1:1 in the scale the scene is drawn in
        const float sens = (m.isShiftDown() ? 0.125f : 1.0f) * dbPerPixel / 60.0f;
        attachment.setValueAsPartOfGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, startNorm + (startY - p.y) * sens)));
    }

    void mouseUp (juce::Point<float>) override { dragging = false; attachment.endGesture(); }

    void mouseDoubleClick (juce::Point<float>) override
    {
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
    }

    void mouseWheel (float dy, const juce::ModifierKeys& m) override
    {
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm + dy * (m.isShiftDown() ? 0.01f : 0.05f))));
    }

    juce::MouseCursor getCursor() const override { return juce::MouseCursor::UpDownResizeCursor; }
    juce::String getReadout() const override { return "IRON = THRESHOLD " + param.getCurrentValueAsText() + " DB"; }

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float norm = 0.0f, startY = 0.0f, startNorm = 0.0f;
    bool dragging = false;
};

//==============================================================================
/** Real-time spectrum: input (dim body), output (bright line), what was ironed away (hot fill in between),
    the detector's listening band and its filter curve. Drag: x = frequency, y = width. FIND S learns the
    sibilance frequency from the playing audio. */
class SpectrumView : public px::Widget
{
public:
    explicit SpectrumView (IronSProcessor& p)
        : proc (p),
          freqP (*p.apvts.getParameter (params::freq)), widthP (*p.apvts.getParameter (params::width)),
          freqA (freqP, [this] (float v) { freqHz = v; }),
          widthA (widthP, [this] (float v) { widthOct = v; })
    {
        freqA.sendInitialUpdate();
        widthA.sendInitialUpdate();
        shownLogFreq = std::log2 (freqHz);
        shownWidth = widthOct;
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
        colIn.fill (silentDb); colOut.fill (silentDb);
    }

    // fed by the editor every frame
    bool band = true;       // band or highpass detection
    float activity = 0.0f;  // 0..1 current reduction

    void update (float dt) override
    {
        shownLogFreq += (std::log2 (freqHz) - shownLogFreq) * px::ease (dt, 16.0f);
        shownWidth   += (widthOct - shownWidth) * px::ease (dt, 16.0f);
        glow         += ((hovered || dragging ? 1.0f : 0.0f) - glow) * px::ease (dt, 10.0f);
        t += dt;
        message = std::max (0.0f, message - dt);

        if ((fftTimer += dt) >= 1.0f / 30.0f)
        {
            analyse (fftTimer);
            fftTimer = 0.0f;
        }

        if (learning && (learnLeft -= dt) <= 0.0f) finishLearning();
    }

    void paint (px::Canvas& c) override
    {
        using namespace px;
        const int x0 = bounds.getX() + 1, y0 = bounds.getY() + 1, iw = innerW(), ih = bounds.getHeight() - 2;
        c.rect (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), mix (pal::iron, ironpal::dim, glow * 0.7f));
        c.fillRect (x0, y0, iw, ih, 0x08070a);

        const float fc = std::exp2 (shownLogFreq);

        // listening zone of the detector
        for (int i = 0; i < iw; ++i)
        {
            const float m = sdsp::DeEsser::detectorMagnitude (xToFreq (i, iw), fc, shownWidth, band);
            if (m < 0.25f) continue;
            for (int y = y0; y < y0 + ih; ++y)
                if (Canvas::bayer (x0 + i, y, 0.25f + 0.25f * m)) c.set (x0 + i, y, mix (0x08070a, ironpal::deep, 0.6f + 0.4f * glow));
        }

        // grid
        for (float f : { 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const int gx = x0 + freqToX (f, iw);
            for (int y = y0; y < y0 + ih; y += 2) c.set (gx, y, 0x17151c);
        }
        font::drawText (c, x0 + freqToX (1000.0f, iw) + 2, y0 + ih - 6, "1K", pal::ash);
        font::drawText (c, x0 + freqToX (10000.0f, iw) + 2, y0 + ih - 6, "10K", pal::ash);

        // spectra
        int prevOut = -1;
        for (int i = 0; i < iw; ++i)
        {
            const int x = x0 + i;
            const int yIn  = dbToY (colIn[(size_t) i], y0, ih);
            const int yOut = std::max (yIn, dbToY (colOut[(size_t) i], y0, ih));
            const int bottom = y0 + ih - 1;

            for (int y = yIn; y <= bottom; ++y)
            {
                const float depth = (float) (y - yIn) / (float) ih;
                if (y < yOut)                                       c.set (x, y, mix (ironpal::heat, ironpal::hot, Canvas::bayer (x, y, 0.5f) ? 0.5f : 0.0f));
                else if (Canvas::bayer (x, y, 0.55f - depth * 0.6f)) c.set (x, y, mix (0x24222b, 0x131218, depth));
            }
            if (yIn < bottom && yIn < yOut) c.set (x, yIn, ironpal::hot);

            if (yOut < bottom)
            {
                if (prevOut >= 0 && std::abs (yOut - prevOut) > 1)
                    c.vline (x, std::min (yOut, prevOut) + 1, std::max (yOut, prevOut) - 1, pal::smoke);
                c.set (x, yOut, pal::bone);
            }
            prevOut = yOut < bottom ? yOut : -1;
        }

        // detector filter curve (0 dB at the top, -36 dB at the bottom)
        for (int i = 0; i < iw; i += 2)
        {
            const float m = sdsp::DeEsser::detectorMagnitude (xToFreq (i, iw), fc, shownWidth, band);
            const float db = 20.0f * std::log10 (std::max (m, 1.0e-4f));
            const int y = y0 + 9 + (int) std::lround (-db / 36.0f * (float) (ih - 10));
            if (y < y0 + ih) c.set (x0 + i, y, mix (ironpal::dim, ironpal::heat, 0.5f + 0.5f * glow));
        }

        // frequency marker: marching dashes and a little iron-tip handle
        const int fx = x0 + freqToX (fc, iw);
        const int off = (int) (t * 10.0f);
        for (int y = y0 + 9; y < y0 + ih; ++y)
            if (((y + off) % 5) < 2) c.blend (fx, y, ironpal::hot, 0.25f + 0.45f * glow + 0.3f * activity);
        const RGB hc = mix (ironpal::heat, ironpal::hot, std::max (glow, activity));
        c.hline (fx - 2, fx + 2, y0 + 9, hc);
        c.hline (fx - 1, fx + 1, y0 + 10, hc);
        c.set (fx, y0 + 11, hc);

        font::drawText (c, x0 + 2, y0 + 2, (params::hzText (fc) + (band ? " BAND" : " HIGH")).toUpperCase().toStdString(),
                        mix (pal::smoke, pal::bone, glow));

        // FIND S button
        const auto br = buttonRect();
        RGB bc = overButton && hovered ? ironpal::hot : mix (pal::smoke, ironpal::heat, 0.4f);
        std::string txt = "FIND S";
        if (learning)
        {
            txt = "LISTEN";
            bc = std::fmod (t, 0.5f) < 0.3f ? ironpal::hot : ironpal::heat;
            const int pw = (int) std::lround ((1.0f - learnLeft / learnSeconds) * (float) (br.getWidth() - 2));
            c.hline (br.getX() + 1, br.getX() + pw, br.getBottom() - 1, ironpal::heat);
        }
        else if (message > 0.0f)
        {
            txt = found ? "FOUND" : "NO S";
            bc = found ? ironpal::hot : pal::bone;
        }
        c.fillRect (br.getX(), br.getY(), br.getWidth(), br.getHeight() - 1, 0x0c0b0f);
        font::drawCentred (c, br.getCentreX(), br.getY() + 1, txt, bc);
    }

    void mouseMove (juce::Point<float> p) override { overButton = buttonRect().toFloat().contains (p); }

    void mouseDown (juce::Point<float> p, const juce::ModifierKeys&) override
    {
        if (buttonRect().toFloat().contains (p))
        {
            if (! learning) startLearning();
            return;
        }
        dragging = true; startY = p.y; startWidth = widthOct;
        freqA.beginGesture(); widthA.beginGesture();
        mouseDrag (p, {});
    }

    void mouseDrag (juce::Point<float> p, const juce::ModifierKeys& m) override
    {
        if (! dragging) return;
        const int iw = innerW();
        const float i = juce::jlimit (0.0f, (float) iw - 1.0f, p.x - (float) bounds.getX() - 1.0f);
        freqA.setValueAsPartOfGesture (juce::jlimit (params::freqMin, params::freqMax, xToFreq ((int) i, iw)));
        const float sens = m.isShiftDown() ? 0.005f : 0.03f;
        widthA.setValueAsPartOfGesture (juce::jlimit (0.3f, 3.0f, startWidth + (startY - p.y) * sens));
    }

    void mouseUp (juce::Point<float>) override
    {
        if (! dragging) return;
        dragging = false; freqA.endGesture(); widthA.endGesture();
    }

    void mouseDoubleClick (juce::Point<float> p) override
    {
        if (buttonRect().toFloat().contains (p)) return;
        freqA.setValueAsCompleteGesture (freqP.convertFrom0to1 (freqP.getDefaultValue()));
        widthA.setValueAsCompleteGesture (widthP.convertFrom0to1 (widthP.getDefaultValue()));
    }

    void mouseWheel (float dy, const juce::ModifierKeys& m) override
    {
        widthA.setValueAsCompleteGesture (juce::jlimit (0.3f, 3.0f, widthOct + dy * (m.isShiftDown() ? 0.05f : 0.3f)));
    }

    juce::MouseCursor getCursor() const override { return juce::MouseCursor::CrosshairCursor; }
    juce::String getReadout() const override
    {
        if (overButton) return learning ? "LISTENING FOR S SOUNDS" : "FIND S: LEARN FREQ FROM AUDIO";
        return "FREQ " + freqP.getCurrentValueAsText() + "  WIDTH " + widthP.getCurrentValueAsText() + " OCT";
    }

    bool isLearning() const noexcept { return learning; }

private:
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, maxCols = 256;
    static constexpr float fLo = 200.0f, fHi = 20000.0f, topDb = -18.0f, bottomDb = -78.0f, silentDb = -140.0f;
    static constexpr float learnSeconds = 3.0f;

    int innerW() const noexcept { return juce::jmin (maxCols, bounds.getWidth() - 2); }
    juce::Rectangle<int> buttonRect() const noexcept { return { bounds.getRight() - 28, bounds.getY() + 1, 27, 8 }; }

    static float xToFreq (float i, int iw) { return fLo * std::pow (fHi / fLo, i / (float) (iw - 1)); }
    static float xToFreq (int i, int iw) { return xToFreq ((float) i, iw); }
    static int freqToX (float f, int iw) { return (int) std::lround (std::log (f / fLo) / std::log (fHi / fLo) * (float) (iw - 1)); }
    static int dbToY (float db, int y0, int ih)
    {
        const float n = juce::jlimit (0.0f, 1.0f, (topDb - db) / (topDb - bottomDb));
        return y0 + (int) std::lround (n * (float) (ih - 1));
    }

    void analyse (float step)
    {
        proc.scope.read (bufIn.data(), bufOut.data(), fftSize);
        for (int i = 0; i < fftSize; ++i)
        {
            bufIn[(size_t) i]  *= window[(size_t) i];
            bufOut[(size_t) i] *= window[(size_t) i];
        }
        std::fill (bufIn.begin() + fftSize, bufIn.end(), 0.0f);
        std::fill (bufOut.begin() + fftSize, bufOut.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (bufIn.data(), true);
        fft.performFrequencyOnlyForwardTransform (bufOut.data(), true);

        const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        binHz = (float) sr / (float) fftSize;
        const int iw = innerW(), maxBin = fftSize / 2 - 1;
        const float up = px::ease (step, 30.0f), down = px::ease (step, 9.0f);

        auto column = [&] (const std::array<float, fftSize * 2>& mag, int i)
        {
            const float b0 = xToFreq ((float) i - 0.5f, iw) / binHz, b1 = xToFreq ((float) i + 0.5f, iw) / binHz;
            float m = 0.0f;
            if (b1 - b0 < 1.0f)
            {
                const float b = juce::jlimit (0.0f, (float) maxBin - 1.0f, 0.5f * (b0 + b1));
                const int k = (int) b;
                m = mag[(size_t) k] + (mag[(size_t) k + 1] - mag[(size_t) k]) * (b - (float) k);
            }
            else
                for (int k = juce::jmax (1, (int) std::ceil (b0)); k <= juce::jmin (maxBin, (int) b1); ++k)
                    m = std::max (m, mag[(size_t) k]);
            // normalised so a full-scale sine reads 0 dB, tilted +3 dB/oct around 1 kHz like most analysers
            return 20.0f * std::log10 (m / (float) (fftSize / 4) + 1.0e-7f) + 3.0f * std::log2 (xToFreq (i, iw) / 1000.0f);
        };

        for (int i = 0; i < iw; ++i)
        {
            const float a = column (bufIn, i), b = column (bufOut, i);
            auto& ci = colIn[(size_t) i];
            auto& co = colOut[(size_t) i];
            ci += (a - ci) * (a > ci ? up : down);
            co += (b - co) * (b > co ? up : down);
        }

        if (learning) learnFrame();
    }

    void startLearning()
    {
        learning = true; learnLeft = learnSeconds; learnFrames = 0;
        learnAcc.fill (0.0f);
    }

    /** Collects the spectrum of frames that are dominated by highs (= sibilants). */
    void learnFrame()
    {
        const int maxBin = fftSize / 2 - 1;
        auto binOf = [&] (float f) { return juce::jlimit (1, maxBin, (int) (f / binHz)); };
        const int bLow = binOf (150.0f), bSplit = binOf (3000.0f), bTop = binOf (16000.0f);
        float lowP = 0.0f, highP = 0.0f;
        for (int k = bLow; k < bSplit; ++k) lowP += bufIn[(size_t) k] * bufIn[(size_t) k];
        for (int k = bSplit; k <= bTop; ++k) highP += bufIn[(size_t) k] * bufIn[(size_t) k];
        const float floorP = (float) fftSize * (float) fftSize * 1.0e-8f;
        if (highP < floorP || highP < 1.5f * lowP) return;
        for (int k = bSplit; k <= bTop; ++k) learnAcc[(size_t) k] += bufIn[(size_t) k] * bufIn[(size_t) k] / highP;
        ++learnFrames;
    }

    void finishLearning()
    {
        learning = false;
        message = 1.8f;
        found = learnFrames >= 3;
        if (! found) return;

        // smooth over +-1/6 octave, then take the strongest region inside the parameter range
        const int maxBin = fftSize / 2 - 1;
        float best = 0.0f, bestF = freqHz;
        for (float f = params::freqMin; f <= params::freqMax; f *= 1.03f)
        {
            const int k0 = juce::jlimit (1, maxBin, (int) (f * 0.89f / binHz)), k1 = juce::jlimit (1, maxBin, (int) (f * 1.12f / binHz));
            float sum = 0.0f;
            for (int k = k0; k <= k1; ++k) sum += learnAcc[(size_t) k];
            sum /= (float) (k1 - k0 + 1);
            const float weighted = sum * f; // undo the natural roll-off a little: per-octave energy
            if (weighted > best) { best = weighted; bestF = f; }
        }
        freqA.setValueAsCompleteGesture (juce::jlimit (params::freqMin, params::freqMax, bestF));
    }

    IronSProcessor& proc;
    juce::RangedAudioParameter& freqP;
    juce::RangedAudioParameter& widthP;
    juce::ParameterAttachment freqA, widthA;
    juce::dsp::FFT fft { fftOrder };

    std::array<float, fftSize> window {};
    std::array<float, fftSize * 2> bufIn {}, bufOut {};
    std::array<float, maxCols> colIn {}, colOut {};
    std::array<float, fftSize / 2> learnAcc {};

    float freqHz = 6500.0f, widthOct = 1.2f, shownLogFreq = 12.67f, shownWidth = 1.2f;
    float glow = 0.0f, t = 0.0f, fftTimer = 0.0f, binHz = 23.4f;
    float startY = 0.0f, startWidth = 1.2f;
    bool dragging = false, overButton = false;
    bool learning = false, found = false;
    float learnLeft = 0.0f, message = 0.0f;
    int learnFrames = 0;
};
