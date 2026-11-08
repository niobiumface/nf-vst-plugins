#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace px
{
using RGB = uint32_t; // 0xRRGGBB

namespace pal
{
    inline constexpr RGB voidC = 0x060608;
    inline constexpr RGB ink   = 0x0e0e13;
    inline constexpr RGB iron  = 0x1b1a22;
    inline constexpr RGB ash   = 0x2e2c36;
    inline constexpr RGB smoke = 0x5a5664;
    inline constexpr RGB bone  = 0xcfc6b4;
    inline constexpr RGB blood = 0xb3121f;
    inline constexpr RGB ember = 0xff3b3b;
    inline constexpr RGB rust  = 0x4a0c12;
}

inline constexpr int r8 (RGB c) { return (int) ((c >> 16) & 0xff); }
inline constexpr int g8 (RGB c) { return (int) ((c >> 8) & 0xff); }
inline constexpr int b8 (RGB c) { return (int) (c & 0xff); }
inline constexpr RGB rgb (int r, int g, int b)
{
    return (RGB) ((std::clamp (r, 0, 255) << 16) | (std::clamp (g, 0, 255) << 8) | std::clamp (b, 0, 255));
}

/** Linear mix a -> b by t (0..1). */
inline RGB mix (RGB a, RGB b, float t)
{
    t = std::clamp (t, 0.0f, 1.0f);
    return rgb ((int) std::lround (r8 (a) + (r8 (b) - r8 (a)) * t),
                (int) std::lround (g8 (a) + (g8 (b) - g8 (a)) * t),
                (int) std::lround (b8 (a) + (b8 (b) - b8 (a)) * t));
}

inline RGB scale (RGB c, float f)
{
    return rgb ((int) (r8 (c) * f), (int) (g8 (c) * f), (int) (b8 (c) * f));
}

/** Frame-rate independent easing factor. */
inline float ease (float dt, float speed) { return 1.0f - std::exp (-dt * speed); }
} // namespace px
