---
name: pixel-ui
description: The shared dark, modern pixel-art UI framework and style guide for the plugins in this repo (shared/pixelui). Use when designing or changing any plugin editor, adding widgets, glyphs, palettes or animations.
---

# Pixel UI framework & style guide

## Principle
Every editor renders into a **low-resolution logical canvas** (e.g. 256×176) and is blitted with
**nearest-neighbour integer scaling** (×2/×3/×4). Never draw anti-aliased vector graphics into the UI —
every visible element is made of real logical pixels. That is what makes it look like pixel art at any size.

## Framework (`shared/pixelui/`, header only, namespace `px`)
- `PixelCanvas.h` – `Canvas` over a `juce::Image`: `set`, `blend`, `fillRect`, `rect`, `hline/vline`, `line` (Bresenham), `disc`, `ring`, `ditherRect` (ordered Bayer dither), glyph blitting.
- `PixelFont.h` – 3×5 font (A–Z, 0–9, symbols) via `drawText(canvas, x, y, text, colour, scale)` and `textWidth`.
- `Palette.h` – the shared dark palette (see below). Plugins may add one accent palette, nothing else.
- `Widget.h` – `px::Widget` base (logical bounds, `paint(Canvas&, float time)`, `update(dt)`, mouse callbacks).
- `Widgets.h` – `Knob` (LED-ring, vertical drag, shift = fine, double-click = default, wheel), `Led­Toggle`, `VMeter` (segmented, peak hold).
- `PixelEditor.h` – `px::PixelEditor : juce::AudioProcessorEditor`: owns canvas + widgets, drives animations from a `juce::VBlankAttachment`, maps mouse coordinates, draws scanlines, handles the scale button.

## Palette (dark modern)
| token | hex | use |
|---|---|---|
| void | #060608 | background |
| ink | #0e0e13 | panels |
| iron | #1b1a22 | knob body, frames |
| ash | #2e2c36 | inactive, grid |
| smoke | #5a5664 | secondary text |
| bone | #cfc6b4 | primary text, indicators |
| blood | #b3121f | accent, active |
| ember | #ff3b3b | highlights, glows (sparingly) |
| rust | #4a0c12 | dim accent, unlit LEDs |

Rules: max one saturated accent per plugin; large areas stay near-black; light comes from the accent
(glows are dithered, never smooth gradients); text is UPPERCASE 3×5.

## Animation rules
- Time-based (`dt` seconds), never frame-count based. Driven by `VBlankAttachment` (vsync, stops when hidden).
- Every animation must react to audio or interaction, or be very subtle ambience (ash particles, flicker).
- Ease displayed values (`v += (target - v) * (1 - exp(-dt * k))`) so knobs and meters glide.
- Budget: whole frame < 1 ms. Canvas is tiny; avoid allocations per frame.

## New editor recipe
1. Subclass `px::PixelEditor`, pass logical size.
2. Create widgets in the constructor, bind knobs to APVTS parameters (uses `juce::ParameterAttachment`).
3. Override `paintBackground` / `paintOverlay` for plugin-specific art (title, scenes).
4. Pixel glyphs for logos are `const char*` row arrays (`#` = on) drawn with `Canvas::glyph`.
