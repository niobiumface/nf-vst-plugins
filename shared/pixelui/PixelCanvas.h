#pragma once
#include <vector>
#include <cstdlib>
#include "Palette.h"

namespace px
{
/** A tiny software framebuffer in logical pixels. Everything in the UI is drawn here. */
class Canvas
{
public:
    Canvas (int width, int height) : w (width), h (height), buf ((size_t) (width * height), pal::voidC) {}

    int width() const noexcept  { return w; }
    int height() const noexcept { return h; }
    const RGB* data() const noexcept { return buf.data(); }

    bool inside (int x, int y) const noexcept { return x >= 0 && y >= 0 && x < w && y < h; }

    void clear (RGB c) { std::fill (buf.begin(), buf.end(), c); }

    void set (int x, int y, RGB c) noexcept { if (inside (x, y)) buf[(size_t) (y * w + x)] = c; }
    RGB  get (int x, int y) const noexcept  { return inside (x, y) ? buf[(size_t) (y * w + x)] : 0; }

    /** Alpha-blend c over the existing pixel. */
    void blend (int x, int y, RGB c, float a) noexcept
    {
        if (inside (x, y)) { auto& p = buf[(size_t) (y * w + x)]; p = mix (p, c, a); }
    }

    /** Adds light (good for glows). */
    void add (int x, int y, RGB c, float a) noexcept
    {
        if (! inside (x, y)) return;
        auto& p = buf[(size_t) (y * w + x)];
        p = rgb (r8 (p) + (int) (r8 (c) * a), g8 (p) + (int) (g8 (c) * a), b8 (p) + (int) (b8 (c) * a));
    }

    void fillRect (int x, int y, int rw, int rh, RGB c) noexcept
    {
        for (int j = y; j < y + rh; ++j)
            for (int i = x; i < x + rw; ++i)
                set (i, j, c);
    }

    void rect (int x, int y, int rw, int rh, RGB c) noexcept
    {
        hline (x, x + rw - 1, y, c); hline (x, x + rw - 1, y + rh - 1, c);
        vline (x, y, y + rh - 1, c); vline (x + rw - 1, y, y + rh - 1, c);
    }

    void hline (int x0, int x1, int y, RGB c) noexcept { for (int x = x0; x <= x1; ++x) set (x, y, c); }
    void vline (int x, int y0, int y1, RGB c) noexcept { for (int y = y0; y <= y1; ++y) set (x, y, c); }

    void line (int x0, int y0, int x1, int y1, RGB c) noexcept
    {
        int dx = std::abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (;;)
        {
            set (x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void disc (int cx, int cy, float r, RGB c) noexcept
    {
        const int ir = (int) std::ceil (r);
        for (int y = -ir; y <= ir; ++y)
            for (int x = -ir; x <= ir; ++x)
                if ((float) (x * x + y * y) <= r * r + 0.5f)
                    set (cx + x, cy + y, c);
    }

    /** 4x4 ordered (Bayer) dither threshold, level 0..1. */
    static bool bayer (int x, int y, float level) noexcept
    {
        static constexpr int m[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
        return level * 16.0f > (float) m[(y & 3) * 4 + (x & 3)] + 0.5f;
    }

    void dither (int x, int y, RGB c, float level) noexcept { if (bayer (x, y, level)) set (x, y, c); }

    void ditherRect (int x, int y, int rw, int rh, RGB c, float level) noexcept
    {
        for (int j = y; j < y + rh; ++j)
            for (int i = x; i < x + rw; ++i)
                dither (i, j, c, level);
    }

    /** Draws a bitmap glyph given as rows of chars ('#' = on), each on-pixel scaled to s x s. */
    void glyph (int x, int y, const char* const* rows, int numRows, RGB c, int s = 1) noexcept
    {
        for (int j = 0; j < numRows; ++j)
            for (int i = 0; rows[j][i] != 0; ++i)
                if (rows[j][i] == '#')
                    fillRect (x + i * s, y + j * s, s, s, c);
    }

private:
    int w, h;
    std::vector<RGB> buf;
};
} // namespace px
