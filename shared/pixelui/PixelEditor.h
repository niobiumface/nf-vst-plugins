#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Widget.h"
#include "PixelFont.h"

namespace px
{
/** Base editor: renders a low-res Canvas, upscales it with nearest-neighbour filtering,
    runs vsync-driven animation and routes mouse events to px::Widgets in logical coordinates. */
class PixelEditor : public juce::AudioProcessorEditor
{
public:
    PixelEditor (juce::AudioProcessor& p, int logicalW, int logicalH, int initialScale)
        : juce::AudioProcessorEditor (p), canvas (logicalW, logicalH),
          image (juce::Image::ARGB, logicalW, logicalH, true, juce::SoftwareImageType())
    {
        setOpaque (true);
        pixelScale = juce::jlimit (minScale, maxScale, initialScale);
        setSize (logicalW * pixelScale, logicalH * pixelScale);
    }

    int getPixelScale() const noexcept { return pixelScale; }
    std::function<void (int)> onScaleChanged;
    bool showScanlines = true;

protected:
    /** Per-frame logic (dt in seconds) — read meters, advance animations. */
    virtual void tick (float /*dt*/) {}
    /** Draw everything behind the widgets. */
    virtual void paintScene (Canvas&) = 0;
    /** Draw everything above the widgets. */
    virtual void paintOverlay (Canvas&) {}

    void addWidget (Widget& w) { widgets.push_back (&w); }
    Widget* getActiveWidget() const noexcept { return captured != nullptr ? captured : hoveredW; }

    float time = 0.0f;
    Canvas canvas;

private:
    static constexpr int minScale = 2, maxScale = 4;

    juce::Rectangle<int> scaleButtonBounds() const { return { canvas.width() - 15, 2, 13, 7 }; }

    void frame (double timestampSec)
    {
        float dt = lastTs > 0.0 ? (float) (timestampSec - lastTs) : 1.0f / 60.0f;
        lastTs = timestampSec;
        dt = juce::jlimit (0.0f, 0.1f, dt);
        time += dt;
        for (auto* w : widgets) w->update (dt);
        tick (dt);
        repaint();
    }

    void render()
    {
        paintScene (canvas);
        for (auto* w : widgets) w->paint (canvas);
        paintOverlay (canvas);

        // scale button (top-right)
        const auto sb = scaleButtonBounds();
        const auto txt = "X" + std::to_string (pixelScale);
        font::drawRight (canvas, sb.getRight(), sb.getY() + 1, txt, scaleHover ? pal::bone : pal::ash);

        juce::Image::BitmapData bd (image, juce::Image::BitmapData::writeOnly);
        const RGB* src = canvas.data();
        for (int y = 0; y < canvas.height(); ++y)
            for (int x = 0; x < canvas.width(); ++x)
            {
                const RGB c = *src++;
                reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (x, y))->setARGB (255, (juce::uint8) r8 (c), (juce::uint8) g8 (c), (juce::uint8) b8 (c));
            }
    }

    void paint (juce::Graphics& g) override
    {
        render();
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (image, getLocalBounds().toFloat());

        if (showScanlines)
        {
            g.setColour (juce::Colours::black.withAlpha (pixelScale >= 3 ? 0.28f : 0.16f));
            for (int y = 0; y < canvas.height(); ++y)
                g.fillRect (0, y * pixelScale + pixelScale - 1, getWidth(), 1);
        }
    }

    juce::Point<float> toLogical (juce::Point<float> p) const { return p / (float) pixelScale; }

    Widget* hit (juce::Point<float> lp) const
    {
        for (auto it = widgets.rbegin(); it != widgets.rend(); ++it)
            if ((*it)->bounds.toFloat().contains (lp)) return *it;
        return nullptr;
    }

    void setHover (Widget* w)
    {
        if (hoveredW == w) return;
        if (hoveredW != nullptr) hoveredW->hovered = false;
        hoveredW = w;
        if (hoveredW != nullptr) hoveredW->hovered = true;
        setMouseCursor (hoveredW != nullptr ? hoveredW->getCursor() : juce::MouseCursor::NormalCursor);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto lp = toLogical (e.position);
        scaleHover = scaleButtonBounds().toFloat().contains (lp);
        setHover (scaleHover ? nullptr : hit (lp));
        if (hoveredW != nullptr) hoveredW->mouseMove (lp);
        if (scaleHover) setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { scaleHover = false; if (captured == nullptr) setHover (nullptr); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto lp = toLogical (e.position);
        if (scaleButtonBounds().toFloat().contains (lp))
        {
            pixelScale = pixelScale >= maxScale ? minScale : pixelScale + 1;
            setSize (canvas.width() * pixelScale, canvas.height() * pixelScale);
            if (onScaleChanged) onScaleChanged (pixelScale);
            return;
        }
        captured = hit (lp);
        if (captured != nullptr) captured->mouseDown (lp, e.mods);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (captured != nullptr) captured->mouseDrag (toLogical (e.position), e.mods);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (captured != nullptr) captured->mouseUp (toLogical (e.position));
        captured = nullptr;
        setHover (hit (toLogical (e.position)));
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (auto* w = hit (toLogical (e.position))) w->mouseDoubleClick (toLogical (e.position));
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wd) override
    {
        if (auto* w = hit (toLogical (e.position))) w->mouseWheel (wd.deltaY, e.mods);
    }

    juce::Image image;
    std::vector<Widget*> widgets;
    Widget* hoveredW = nullptr;
    Widget* captured = nullptr;
    bool scaleHover = false;
    int pixelScale = 3;
    double lastTs = 0.0;
    juce::VBlankAttachment vblank { this, [this] (double ts) { frame (ts); } };
};
} // namespace px
