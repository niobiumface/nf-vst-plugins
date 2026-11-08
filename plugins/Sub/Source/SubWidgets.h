#pragma once
#include <pixelui/Widgets.h>
#include <dsp/StackedLowpass.h>
#include "Params.h"

/** Sub's single accent: a sulphur violet. */
namespace subpal
{
inline constexpr px::RGB vio  = 0x7b2fd0; // accent
inline constexpr px::RGB hot  = 0xd29bff; // glow highlight
inline constexpr px::RGB dim  = 0x2b1245; // unlit / dim accent
inline constexpr px::RGB deep = 0x12081c; // accent-tinted black
}

inline float smooth01 (float a, float b, float x) noexcept
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

//==============================================================================
/** Invisible hit area over the nun: vertical drag pulls the cutoff (down = darker). */
class NunWidget : public px::Widget
{
public:
    explicit NunWidget (juce::RangedAudioParameter& p)
        : param (p), attachment (p, [this] (float v) { norm = param.convertTo0to1 (v); })
    {
        attachment.sendInitialUpdate();
    }

    std::function<void()> onPoke;
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
        const float sens = m.isShiftDown() ? 0.0008f : 0.005f;
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
    juce::String getReadout() const override { return "CUTOFF " + param.getCurrentValueAsText() + " HZ"; }

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float norm = 0.0f, startY = 0.0f, startNorm = 0.0f;
    bool dragging = false;
};

//==============================================================================
/** Filter response curve. Drag: x = cutoff, y = resonance. */
class ResponseView : public px::Widget
{
public:
    ResponseView (juce::RangedAudioParameter& cut, juce::RangedAudioParameter& res)
        : cutP (cut), resP (res),
          cutA (cut, [this] (float v) { cutHz = v; }),
          resA (res, [this] (float v) { reso = v * 0.01f; })
    {
        cutA.sendInitialUpdate();
        resA.sendInitialUpdate();
        shownLogCut = std::log2 (cutHz);
        shownReso = reso;
    }

    // fed by the editor every frame
    float layerPos = 2.0f, wetMix = 1.0f, level = 0.0f, darkness = 0.0f;

    void update (float dt) override
    {
        shownLogCut += (std::log2 (cutHz) - shownLogCut) * px::ease (dt, 16.0f);
        shownReso   += (reso - shownReso) * px::ease (dt, 16.0f);
        glow        += ((hovered || dragging ? 1.0f : 0.0f) - glow) * px::ease (dt, 10.0f);
        t += dt;
    }

    void paint (px::Canvas& c) override
    {
        using namespace px;
        const int x0 = bounds.getX() + 1, y0 = bounds.getY() + 1, iw = bounds.getWidth() - 2, ih = bounds.getHeight() - 2;
        c.rect (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), mix (pal::iron, subpal::dim, glow * 0.7f));
        c.fillRect (x0, y0, iw, ih, 0x08070b);

        // grid: decades + 0 dB
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const int gx = x0 + freqToX (f, iw);
            for (int y = y0; y < y0 + ih; y += 2) c.set (gx, y, 0x15131b);
        }
        const int zeroY = dbToY (0.0f, y0, ih);
        for (int x = x0; x < x0 + iw; x += 2) c.set (x, zeroY, 0x1d1a24);

        const float fc = std::exp2 (shownLogCut);
        const int cutX = x0 + freqToX (fc, iw);
        int prevY = -1;
        for (int i = 0; i < iw; ++i)
        {
            const float f = xToFreq (i, iw);
            const float m = sdsp::StackedLowpass::magnitude (f, fc, shownReso, layerPos);
            const float total = wetMix * m + (1.0f - wetMix);
            const float db = 20.0f * std::log10 (std::max (total, 1.0e-4f));
            const int y = dbToY (db, y0, ih);
            const int x = x0 + i;

            // dithered body under the curve, breathing with the audio
            const float bodyLvl = (0.22f + 0.35f * level) * (1.0f - (float) i / (float) iw * 0.4f);
            for (int yy = std::max (y + 1, y0); yy < y0 + ih; ++yy)
            {
                const float depth = (float) (yy - y) / (float) ih;
                if (Canvas::bayer (x, yy + (int) (t * 6.0f), bodyLvl * (1.0f - depth)))
                    c.set (x, yy, mix (subpal::deep, subpal::dim, 1.0f - depth));
            }

            const float prox = 1.0f - juce::jlimit (0.0f, 1.0f, std::abs ((float) (x - cutX)) / 14.0f);
            const RGB lineCol = mix (subpal::vio, subpal::hot, prox * (0.4f + 0.6f * glow));
            if (y >= y0 && y < y0 + ih)
            {
                if (prevY >= 0 && std::abs (y - prevY) > 1)
                    c.vline (x, std::max (y0, std::min (y, prevY)), std::min (y0 + ih - 1, std::max (y, prevY)), lineCol);
                c.set (x, y, lineCol);
            }
            prevY = juce::jlimit (y0, y0 + ih - 1, y);
        }

        // cutoff marker: marching dashed line + a tiny inverted cross handle
        const int off = (int) (t * 10.0f);
        for (int y = y0; y < y0 + ih; ++y)
            if (((y + off) % 5) < 2) c.blend (cutX, y, subpal::hot, 0.25f + 0.5f * glow);
        const float mPeak = wetMix * sdsp::StackedLowpass::magnitude (fc, fc, shownReso, layerPos) + (1.0f - wetMix);
        const int hy = juce::jlimit (y0 + 2, y0 + ih - 5, dbToY (20.0f * std::log10 (std::max (mPeak, 1.0e-4f)), y0, ih) - 2);
        const RGB hc = mix (subpal::vio, subpal::hot, 0.5f + 0.5f * glow);
        c.vline (cutX, hy - 2, hy + 2, hc);
        c.hline (cutX - 1, cutX + 1, hy + 1, hc);

        font::drawText (c, x0 + 2, y0 + 2, (juce::String (juce::roundToInt (layerPos * 12.0f)) + " DB/OCT").toStdString(),
                        mix (pal::ash, pal::smoke, glow));
    }

    void mouseDown (juce::Point<float> p, const juce::ModifierKeys&) override
    {
        dragging = true; startY = p.y; startReso = reso;
        cutA.beginGesture(); resA.beginGesture();
        mouseDrag (p, {});
    }

    void mouseDrag (juce::Point<float> p, const juce::ModifierKeys& m) override
    {
        const int iw = bounds.getWidth() - 2;
        const float i = juce::jlimit (0.0f, (float) iw - 1.0f, p.x - (float) bounds.getX() - 1.0f);
        cutA.setValueAsPartOfGesture (juce::jlimit (20.0f, 20000.0f, 20.0f * std::pow (1000.0f, i / (float) (iw - 1))));
        const float sens = m.isShiftDown() ? 0.002f : 0.012f;
        resA.setValueAsPartOfGesture (juce::jlimit (0.0f, 1.0f, startReso + (startY - p.y) * sens) * 100.0f);
    }

    void mouseUp (juce::Point<float>) override { dragging = false; cutA.endGesture(); resA.endGesture(); }

    void mouseDoubleClick (juce::Point<float>) override
    {
        cutA.setValueAsCompleteGesture (cutP.convertFrom0to1 (cutP.getDefaultValue()));
        resA.setValueAsCompleteGesture (resP.convertFrom0to1 (resP.getDefaultValue()));
    }

    void mouseWheel (float dy, const juce::ModifierKeys& m) override
    {
        resA.setValueAsCompleteGesture (juce::jlimit (0.0f, 100.0f, reso * 100.0f + dy * (m.isShiftDown() ? 2.0f : 10.0f)));
    }

    juce::MouseCursor getCursor() const override { return juce::MouseCursor::CrosshairCursor; }
    juce::String getReadout() const override
    {
        return "CUT " + cutP.getCurrentValueAsText() + "  RES " + resP.getCurrentValueAsText() + "%";
    }

private:
    static float xToFreq (int i, int iw) { return 20.0f * std::pow (1000.0f, (float) i / (float) (iw - 1)); }
    static int freqToX (float f, int iw) { return (int) std::lround (std::log (f / 20.0f) / std::log (1000.0f) * (float) (iw - 1)); }
    static int dbToY (float db, int y0, int ih)
    {
        constexpr float top = 18.0f, bottom = -54.0f;
        const float n = (top - db) / (top - bottom);
        return y0 + (int) std::lround (n * (float) (ih - 1));
    }

    juce::RangedAudioParameter& cutP;
    juce::RangedAudioParameter& resP;
    juce::ParameterAttachment cutA, resA;
    float cutHz = 1000.0f, reso = 0.0f, shownLogCut = 10.0f, shownReso = 0.0f, glow = 0.0f, t = 0.0f;
    bool dragging = false;
    float startY = 0.0f, startReso = 0.0f;
};

//==============================================================================
/** Four clickable sigils, one per 12 dB/oct layer. */
class LayerSelector : public px::Widget
{
public:
    explicit LayerSelector (juce::RangedAudioParameter& p)
        : param (p), attachment (p, [this] (float v) { layers = juce::jlimit (1, 4, juce::roundToInt (v)); })
    {
        attachment.sendInitialUpdate();
        for (int i = 0; i < 4; ++i) lit[(size_t) i] = i < layers ? 1.0f : 0.0f;
    }

    float level = 0.0f; // audio activity, makes lit sigils throb

    void update (float dt) override
    {
        for (int i = 0; i < 4; ++i)
            lit[(size_t) i] += ((i < layers ? 1.0f : 0.0f) - lit[(size_t) i]) * px::ease (dt, 12.0f - (float) i * 1.5f);
        t += dt;
    }

    void paint (px::Canvas& c) override
    {
        using namespace px;
        const int y = bounds.getY();
        font::drawText (c, bounds.getX(), y + 2, "LAYERS", hovered ? pal::bone : pal::smoke);
        for (int i = 0; i < 4; ++i)
        {
            const int sx = slotX (i);
            const float l = lit[(size_t) i];
            const float throb = 0.75f + 0.25f * level * (0.5f + 0.5f * std::sin (t * 9.0f - (float) i));
            const bool hv = hovered && hoverSlot == i;
            RGB col = mix (subpal::dim, subpal::vio, l * throb);
            if (hv) col = mix (col, subpal::hot, 0.6f);
            // inverted cross sigil (5 x 9)
            c.vline (sx + 2, y, y + 8, col);
            c.hline (sx, sx + 4, y + 6, col);
            if (l > 0.5f)
            {
                c.set (sx + 2, y + 6, mix (col, subpal::hot, 0.6f * l));
                for (int j = -1; j <= 9; ++j)
                    for (int k = -2; k <= 6; ++k)
                        if (Canvas::bayer (sx + k, y + j, 0.12f * l * throb)) c.add (sx + k, y + j, subpal::vio, 0.18f);
            }
        }
        font::drawRight (c, bounds.getRight(), y + 2, std::to_string (layers * 12) + " DB",
                         mix (pal::smoke, pal::bone, hovered ? 1.0f : 0.5f));
    }

    void mouseMove (juce::Point<float> p) override { hoverSlot = slotAt (p); }

    void mouseDown (juce::Point<float> p, const juce::ModifierKeys&) override
    {
        const int s = slotAt (p);
        if (s >= 0) attachment.setValueAsCompleteGesture ((float) (s + 1));
        else        attachment.setValueAsCompleteGesture ((float) (layers % 4 + 1));
    }

    void mouseWheel (float dy, const juce::ModifierKeys&) override
    {
        attachment.setValueAsCompleteGesture ((float) juce::jlimit (1, 4, layers + (dy > 0.0f ? 1 : -1)));
    }

    juce::MouseCursor getCursor() const override { return juce::MouseCursor::PointingHandCursor; }
    juce::String getReadout() const override { return "LAYERS " + juce::String (layers) + " = " + juce::String (layers * 12) + " DB/OCT"; }

private:
    int slotX (int i) const noexcept { return bounds.getX() + 34 + i * 14; }
    int slotAt (juce::Point<float> p) const noexcept
    {
        for (int i = 0; i < 4; ++i)
            if (p.x >= (float) slotX (i) - 4.0f && p.x < (float) slotX (i) + 9.0f) return i;
        return -1;
    }

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    int layers = 2, hoverSlot = -1;
    std::array<float, 4> lit {};
    float t = 0.0f;
};
