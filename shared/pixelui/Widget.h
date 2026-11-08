#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PixelCanvas.h"

namespace px
{
/** Base for everything interactive drawn on the pixel canvas. Coordinates are logical pixels. */
class Widget
{
public:
    virtual ~Widget() = default;

    juce::Rectangle<int> bounds;
    bool hovered = false;
    bool enabled = true;

    virtual void update (float /*dt*/) {}
    virtual void paint (Canvas&) = 0;

    virtual void mouseMove (juce::Point<float>) {}
    virtual void mouseDown (juce::Point<float>, const juce::ModifierKeys&) {}
    virtual void mouseDrag (juce::Point<float>, const juce::ModifierKeys&) {}
    virtual void mouseUp (juce::Point<float>) {}
    virtual void mouseDoubleClick (juce::Point<float>) {}
    virtual void mouseWheel (float /*deltaY*/, const juce::ModifierKeys&) {}

    virtual juce::MouseCursor getCursor() const { return juce::MouseCursor::NormalCursor; }
    /** Text shown in a readout line while hovered/dragged, e.g. "THRESH -40.0 DB". */
    virtual juce::String getReadout() const { return {}; }
};
} // namespace px
