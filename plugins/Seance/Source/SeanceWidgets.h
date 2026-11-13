#pragma once
#include <pixelui/Widget.h>
#include <pixelui/Palette.h>

/** Séance's accent: ectoplasm. Everything else comes from the shared palette (plus the board's wood and candle tones). */
namespace seancepal
{
inline constexpr px::RGB ecto = 0x8ef5d2; // accent
inline constexpr px::RGB hot  = 0xe8fff7; // ghost-white highlight
inline constexpr px::RGB dim  = 0x173d35; // unlit / dim accent
inline constexpr px::RGB deep = 0x0a1a17; // accent-tinted black
}

/** Invisible handle over the whole board: drag left/right = delay time (or the note value when synced),
    up/down = feedback. The editor draws the board and the planchette. */
class BoardHandle : public px::Widget
{
public:
    BoardHandle (juce::RangedAudioParameter& timeP, juce::RangedAudioParameter& divP,
                 juce::RangedAudioParameter& fbP, juce::RangedAudioParameter& syncP)
        : time (timeP), div (divP), fb (fbP), sync (syncP),
          timeA (timeP, [this] (float v) { timeN = time.convertTo0to1 (v); }),
          divA (divP, [this] (float v) { divN = div.convertTo0to1 (v); }),
          fbA (fbP, [this] (float v) { fbN = fb.convertTo0to1 (v); })
    {
        timeA.sendInitialUpdate();
        divA.sendInitialUpdate();
        fbA.sendInitialUpdate();
    }

    std::function<void()> onPoke;

    void update (float dt) override { glow += ((hovered || dragging ? 1.0f : 0.0f) - glow) * px::ease (dt, 10.0f); }
    void paint (px::Canvas&) override {}

    void mouseDown (juce::Point<float> p, const juce::ModifierKeys&) override
    {
        dragging = true;
        start = p;
        startX = synced() ? divN : timeN;
        startFb = fbN;
        xAtt().beginGesture();
        fbA.beginGesture();
        if (onPoke) onPoke();
    }

    void mouseDrag (juce::Point<float> p, const juce::ModifierKeys& m) override
    {
        const float sx = m.isShiftDown() ? 0.0006f : 0.004f, sy = m.isShiftDown() ? 0.001f : 0.007f;
        auto& xp = synced() ? div : time;
        xAtt().setValueAsPartOfGesture (xp.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, startX + (p.x - start.x) * sx)));
        fbA.setValueAsPartOfGesture (fb.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, startFb + (start.y - p.y) * sy)));
    }

    void mouseUp (juce::Point<float>) override
    {
        dragging = false;
        xAtt().endGesture();
        fbA.endGesture();
    }

    void mouseDoubleClick (juce::Point<float>) override
    {
        auto& xp = synced() ? div : time;
        xAtt().setValueAsCompleteGesture (xp.convertFrom0to1 (xp.getDefaultValue()));
        fbA.setValueAsCompleteGesture (fb.convertFrom0to1 (fb.getDefaultValue()));
    }

    void mouseWheel (float dy, const juce::ModifierKeys& m) override
    {
        fbA.setValueAsCompleteGesture (fb.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, fbN + dy * (m.isShiftDown() ? 0.01f : 0.05f))));
    }

    juce::MouseCursor getCursor() const override { return juce::MouseCursor::UpDownLeftRightResizeCursor; }
    juce::String getReadout() const override
    {
        const auto t = synced() ? div.getCurrentValueAsText() : time.getCurrentValueAsText() + " MS";
        return "TIME " + t + "  FDBK " + fb.getCurrentValueAsText() + "%";
    }

    bool isDragging() const noexcept { return dragging; }
    float getGlow() const noexcept { return glow; }

private:
    bool synced() const { return sync.getValue() > 0.5f; }
    juce::ParameterAttachment& xAtt() { return synced() ? divA : timeA; }

    juce::RangedAudioParameter& time;
    juce::RangedAudioParameter& div;
    juce::RangedAudioParameter& fb;
    juce::RangedAudioParameter& sync;
    juce::ParameterAttachment timeA, divA, fbA;
    float timeN = 0.0f, divN = 0.0f, fbN = 0.0f, glow = 0.0f, startX = 0.0f, startFb = 0.0f;
    juce::Point<float> start;
    bool dragging = false;
};

/** Shows and routes input to one of two widgets in the same slot (TIME knob / DIVISION knob). */
class SwapWidget : public px::Widget
{
public:
    SwapWidget (px::Widget& first, px::Widget& second, std::function<bool()> useSecond)
        : a (first), b (second), pickSecond (std::move (useSecond)) {}

    void update (float dt) override
    {
        a.bounds = b.bounds = bounds;
        auto& o = other();
        if (o.hovered) o.hovered = false;
        cur().hovered = hovered;
        a.update (dt);
        b.update (dt);
    }

    void paint (px::Canvas& c) override { cur().paint (c); }
    void mouseMove (juce::Point<float> p) override { cur().mouseMove (p); }
    void mouseDown (juce::Point<float> p, const juce::ModifierKeys& m) override { captured = &cur(); captured->mouseDown (p, m); }
    void mouseDrag (juce::Point<float> p, const juce::ModifierKeys& m) override { if (captured) captured->mouseDrag (p, m); }
    void mouseUp (juce::Point<float> p) override { if (captured) captured->mouseUp (p); captured = nullptr; }
    void mouseDoubleClick (juce::Point<float> p) override { cur().mouseDoubleClick (p); }
    void mouseWheel (float dy, const juce::ModifierKeys& m) override { cur().mouseWheel (dy, m); }
    juce::MouseCursor getCursor() const override { return const_cast<SwapWidget*> (this)->cur().getCursor(); }
    juce::String getReadout() const override { return const_cast<SwapWidget*> (this)->cur().getReadout(); }

private:
    px::Widget& cur() { return captured != nullptr ? *captured : (pickSecond() ? b : a); }
    px::Widget& other() { return &cur() == &a ? b : a; }

    px::Widget& a;
    px::Widget& b;
    std::function<bool()> pickSecond;
    px::Widget* captured = nullptr;
};
