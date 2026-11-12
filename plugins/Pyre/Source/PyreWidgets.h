#pragma once
#include <pixelui/Widget.h>
#include <pixelui/Palette.h>

/** Pyre's accent: flame gold. Everything else comes from the shared palette (plus the fire ramp in the editor). */
namespace pyrepal
{
inline constexpr px::RGB flame = 0xffb42e; // accent
inline constexpr px::RGB hot   = 0xfff1c4; // white-hot highlight
inline constexpr px::RGB dim   = 0x4a2a08; // unlit / dim accent
inline constexpr px::RGB deep  = 0x1a0e04; // accent-tinted black
}

/** Invisible handle over the whole stage: drag the fire up/down to set the drive. The editor draws the fire. */
class FireHandle : public px::Widget
{
public:
    explicit FireHandle (juce::RangedAudioParameter& p)
        : param (p), attachment (p, [this] (float v) { norm = param.convertTo0to1 (v); })
    {
        attachment.sendInitialUpdate();
    }

    std::function<void()> onPoke;

    void update (float dt) override { glow += ((hovered || dragging ? 1.0f : 0.0f) - glow) * px::ease (dt, 10.0f); }
    void paint (px::Canvas&) override {}

    void mouseDown (juce::Point<float> p, const juce::ModifierKeys&) override
    {
        dragging = true; startY = p.y; startNorm = norm;
        attachment.beginGesture();
        if (onPoke) onPoke();
    }

    void mouseDrag (juce::Point<float> p, const juce::ModifierKeys& m) override
    {
        const float sens = m.isShiftDown() ? 0.001f : 0.006f;
        attachment.setValueAsPartOfGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, startNorm + (startY - p.y) * sens)));
    }

    void mouseUp (juce::Point<float>) override { dragging = false; attachment.endGesture(); }

    void mouseDoubleClick (juce::Point<float>) override
    {
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
    }

    void mouseWheel (float dy, const juce::ModifierKeys& m) override
    {
        const float n = juce::jlimit (0.0f, 1.0f, norm + dy * (m.isShiftDown() ? 0.01f : 0.05f));
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (n));
    }

    juce::MouseCursor getCursor() const override { return juce::MouseCursor::UpDownResizeCursor; }
    juce::String getReadout() const override { return "DRIVE " + param.getCurrentValueAsText() + " DB"; }

    bool isDragging() const noexcept { return dragging; }
    float getGlow() const noexcept { return glow; }

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float norm = 0.0f, glow = 0.0f, startY = 0.0f, startNorm = 0.0f;
    bool dragging = false;
};
