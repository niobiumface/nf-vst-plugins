#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Widget.h"
#include "PixelFont.h"

namespace px
{
//==============================================================================
/** Pixel knob with an LED ring. Vertical drag, shift = fine, double-click = default, wheel. */
class Knob : public Widget
{
public:
    Knob (juce::RangedAudioParameter& p, juce::String shortLabel, juce::String longName, juce::String unitText,
          RGB accentColour = pal::blood)
        : param (p), label (std::move (shortLabel)), name (std::move (longName)), unit (std::move (unitText)), accent (accentColour),
          attachment (p, [this] (float v) { norm = param.convertTo0to1 (v); })
    {
        attachment.sendInitialUpdate();
        shown = norm;
    }

    void update (float dt) override
    {
        shown += (norm - shown) * ease (dt, 18.0f);
        glow  += ((hovered || dragging ? 1.0f : 0.0f) - glow) * ease (dt, 10.0f);
        t += dt;
    }

    void paint (Canvas& c) override
    {
        const int cx = bounds.getCentreX(), cy = bounds.getY() + 12;
        const float dim = enabled ? 1.0f : 0.45f;

        // LED ring: 13 dots over 270 degrees
        constexpr int dots = 13;
        const int litIdx = (int) std::lround (shown * (dots - 1));
        for (int i = 0; i < dots; ++i)
        {
            const float a = juce::degreesToRadians (-135.0f + 270.0f * (float) i / (dots - 1));
            const int x = cx + (int) std::lround (std::sin (a) * 11.0f);
            const int y = cy - (int) std::lround (std::cos (a) * 11.0f);
            RGB col = unlit;
            if (i <= litIdx)
            {
                const float pulse = 0.5f + 0.5f * std::sin (t * 6.0f - (float) i * 0.6f);
                col = mix (accent, hot, i == litIdx ? 0.7f + 0.3f * glow : glow * 0.35f * pulse);
            }
            c.set (x, y, scale (col, dim));
        }

        // body
        c.disc (cx, cy, 8.0f, scale (pal::iron, 1.0f));
        for (int y = -8; y <= 8; ++y)
            for (int x = -8; x <= 8; ++x)
            {
                const float d = std::sqrt ((float) (x * x + y * y));
                if (d > 8.4f) continue;
                if (d > 7.2f) c.set (cx + x, cy + y, (x + y) < 0 ? mix (pal::ash, pal::smoke, glow * 0.4f) : pal::voidC);
                else if (d < 5.5f && (x + y) < -4) c.set (cx + x, cy + y, mix (pal::iron, pal::ash, 0.35f));
            }

        // indicator
        const float a = juce::degreesToRadians (-135.0f + 270.0f * shown);
        const int ex = cx + (int) std::lround (std::sin (a) * 6.0f);
        const int ey = cy - (int) std::lround (std::cos (a) * 6.0f);
        c.line (cx, cy, ex, ey, scale (pal::bone, dim));
        c.set (ex, ey, scale (dragging ? hot : pal::bone, dim));

        // label / value
        const auto txt = (hovered || dragging) ? valueText() : label;
        font::drawCentred (c, cx, bounds.getY() + 26, txt.toStdString(),
                           scale ((hovered || dragging) ? pal::bone : pal::smoke, dim));
    }

    void mouseDown (juce::Point<float> p, const juce::ModifierKeys&) override
    {
        dragging = true; startY = p.y; startNorm = norm;
        attachment.beginGesture();
    }

    void mouseDrag (juce::Point<float> p, const juce::ModifierKeys& m) override
    {
        const float sens = m.isShiftDown() ? 0.0012f : 0.008f;
        setNorm (startNorm + (startY - p.y) * sens, false);
    }

    void mouseUp (juce::Point<float>) override { dragging = false; attachment.endGesture(); }

    void mouseDoubleClick (juce::Point<float>) override
    {
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
    }

    void mouseWheel (float dy, const juce::ModifierKeys& m) override
    {
        setNorm (norm + dy * (m.isShiftDown() ? 0.01f : 0.05f), true);
    }

    juce::MouseCursor getCursor() const override { return juce::MouseCursor::UpDownResizeCursor; }
    juce::String getReadout() const override { return name + " " + valueText() + (unit.isNotEmpty() ? " " + unit : ""); }

    float getShownValue() const noexcept { return shown; }
    bool isDragging() const noexcept { return dragging; }

    RGB hot = pal::ember; // highlight / glow colour of the ring
    RGB unlit = pal::rust; // unlit ring dots

private:
    juce::String valueText() const { return param.getCurrentValueAsText(); }

    void setNorm (float n, bool complete)
    {
        n = juce::jlimit (0.0f, 1.0f, n);
        const float v = param.convertFrom0to1 (n);
        if (complete) attachment.setValueAsCompleteGesture (v);
        else          attachment.setValueAsPartOfGesture (v);
    }

    juce::RangedAudioParameter& param;
    juce::String label, name, unit;
    RGB accent;
    float norm = 0.0f, shown = 0.0f, glow = 0.0f, t = 0.0f;
    bool dragging = false;
    float startY = 0.0f, startNorm = 0.0f;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** LED + label toggle bound to a bool parameter. */
class LedToggle : public Widget
{
public:
    LedToggle (juce::RangedAudioParameter& p, juce::String text, RGB accentColour = pal::blood)
        : param (p), label (std::move (text)), accent (accentColour),
          attachment (p, [this] (float v) { on = v > 0.5f; })
    {
        attachment.sendInitialUpdate();
        level = on ? 1.0f : 0.0f;
    }

    bool isOn() const noexcept { return on; }
    float getLevel() const noexcept { return level; }
    float pulse = 0.0f; // optional external activity (0..1) to make the LED throb

    void update (float dt) override
    {
        level += ((on ? 1.0f : 0.0f) - level) * ease (dt, 14.0f);
        t += dt;
    }

    void paint (Canvas& c) override
    {
        const int x = bounds.getX(), y = bounds.getY();
        const float throb = level * (0.75f + 0.25f * pulse * (0.5f + 0.5f * std::sin (t * 20.0f)));
        // halo
        for (int j = -2; j <= 4; ++j)
            for (int i = -2; i <= 4; ++i)
            {
                const float d = std::sqrt ((float) ((i - 1) * (i - 1) + (j - 1) * (j - 1)));
                if (d > 1.0f && d < 3.3f) c.add (x + i, y + j, accent, 0.22f * throb * (1.0f - d / 3.3f));
            }
        c.fillRect (x, y, 3, 3, mix (pal::rust, accent, throb));
        c.set (x, y, mix (pal::rust, pal::ember, throb));
        font::drawText (c, x + 6, y - 1, label.toStdString(), mix (pal::smoke, pal::bone, level * (hovered ? 1.0f : 0.85f)));
    }

    void mouseDown (juce::Point<float>, const juce::ModifierKeys&) override
    {
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    }

    juce::MouseCursor getCursor() const override { return juce::MouseCursor::PointingHandCursor; }
    juce::String getReadout() const override { return label + (on ? " ON" : " OFF"); }

private:
    juce::RangedAudioParameter& param;
    juce::String label;
    RGB accent;
    bool on = false;
    float level = 0.0f, t = 0.0f;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** "< NAME >" stepper (e.g. presets). Arrows step, clicking the name calls onClick (e.g. a menu). */
class Selector : public Widget
{
public:
    std::function<juce::String()> getText;
    std::function<void (int)> onStep;
    std::function<void()> onClick;
    RGB accent = pal::blood;

    /** Call when the selection changed to play the flash animation. */
    void flashNow() noexcept { flash = 1.0f; }

    void update (float dt) override
    {
        flash = std::max (0.0f, flash - dt * 2.5f);
        t += dt;
    }

    void paint (Canvas& c) override
    {
        const int x = bounds.getX(), y = bounds.getY() + 1, r = bounds.getRight() - 1;
        const auto txt = (getText ? getText() : juce::String()).toUpperCase().toStdString();
        const float nudge = std::sin (t * 4.0f) > 0.0f ? 1.0f : 0.0f;
        font::drawText (c, x - (zone == -1 ? (int) nudge : 0), y, "<", zone == -1 ? pal::ember : pal::smoke);
        font::drawRight (c, r + (zone == 1 ? (int) nudge : 0), y, ">", zone == 1 ? pal::ember : pal::smoke);
        const RGB col = mix (hovered && zone == 0 ? pal::bone : mix (pal::smoke, pal::bone, 0.6f), pal::ember, flash);
        font::drawCentred (c, bounds.getCentreX(), y, txt, col);
        if (hovered && zone == 0)
            for (int i = x + 6; i <= r - 6; ++i) c.dither (i, y + 6, accent, 0.5f);
    }

    void mouseDown (juce::Point<float> p, const juce::ModifierKeys&) override
    {
        zone = zoneAt (p);
        if (zone != 0) { if (onStep) onStep (zone); flashNow(); }
        else if (onClick) onClick();
    }

    void mouseMove (juce::Point<float> p) override { zone = zoneAt (p); }

    juce::MouseCursor getCursor() const override { return juce::MouseCursor::PointingHandCursor; }

    int zone = 0; // -1 prev, 0 name, +1 next

private:
    int zoneAt (juce::Point<float> p) const noexcept
    {
        if (p.x < (float) bounds.getX() + 8.0f) return -1;
        if (p.x > (float) bounds.getRight() - 8.0f) return 1;
        return 0;
    }

    float flash = 0.0f, t = 0.0f;
};

//==============================================================================
/** Segmented vertical meter with peak hold. Feed it a dB value each frame. */
class VMeter : public Widget
{
public:
    enum class Mode { level, reduction };

    VMeter (juce::String text, Mode m, float minDb) : label (std::move (text)), mode (m), floorDb (minDb) {}

    void setDb (float db) noexcept { target = db; }
    float markerDb = 1.0f; // > 0 = hidden

    void update (float dt) override
    {
        const float tn = norm (target);
        if (mode == Mode::level) shown = tn > shown ? tn : shown + (tn - shown) * ease (dt, 6.0f);
        else                     shown = tn > shown ? shown + (tn - shown) * ease (dt, 40.0f) : shown + (tn - shown) * ease (dt, 5.0f);

        if (shown >= peak) { peak = shown; peakHold = 0.8f; }
        else if ((peakHold -= dt) < 0.0f) peak = std::max (shown, peak - dt * 0.5f);
    }

    void paint (Canvas& c) override
    {
        const int x = bounds.getX(), y = bounds.getY(), w = bounds.getWidth(), h = bounds.getHeight() - 8;
        c.rect (x - 1, y - 1, w + 2, h + 2, pal::iron);
        c.fillRect (x, y, w, h, 0x08080b);
        const int segs = h / 2;
        for (int s = 0; s < segs; ++s)
        {
            const float pos = (float) (s + 1) / (float) segs; // 0 bottom -> 1 top
            const int sy = (mode == Mode::level) ? y + h - (s + 1) * 2 : y + s * 2;
            const bool lit = pos <= shown + 0.001f;
            RGB col;
            if (mode == Mode::level) col = pos > 0.9f ? pal::ember : pos > 0.7f ? pal::blood : mix (pal::smoke, pal::bone, pos);
            else                     col = mix (pal::blood, pal::ember, pos);
            c.hline (x, x + w - 1, sy, lit ? col : (enabled ? 0x141218 : 0x0c0b0e));
        }
        if (peak > 0.02f)
        {
            const int s = std::max (0, (int) std::lround (peak * segs) - 1);
            const int py = (mode == Mode::level) ? y + h - (s + 1) * 2 : y + s * 2;
            c.hline (x, x + w - 1, py, pal::bone);
        }
        if (markerDb <= 0.0f)
        {
            const int my = y + h - (int) std::lround (norm (markerDb) * h);
            c.set (x - 2, my, pal::bone); c.set (x + w + 1, my, pal::bone);
        }
        font::drawCentred (c, bounds.getCentreX(), y + h + 3, label.toStdString(), pal::smoke);
    }

private:
    float norm (float db) const noexcept
    {
        if (mode == Mode::level) return juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb);
        return juce::jlimit (0.0f, 1.0f, -db / -floorDb); // reduction: 0 dB -> 0, floorDb -> 1
    }

    juce::String label;
    Mode mode;
    float floorDb;
    float target = -100.0f, shown = 0.0f, peak = 0.0f, peakHold = 0.0f;
};
} // namespace px
