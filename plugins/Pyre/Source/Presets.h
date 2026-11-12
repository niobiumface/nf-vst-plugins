#pragma once
#include <array>
#include "Params.h"

namespace presets
{
struct Preset
{
    const char* name;
    float drive;
    int mode; // 0 tube, 1 tape, 2 fuzz, 3 fold
    float bias, tone, mix;
};

// Factory presets. Order is permanent once released (hosts store the index).
inline constexpr Preset factory[] = {
    //  name             drive  mode  bias    tone    mix
    { "INIT",            9.0f,  0,    0.0f,   0.0f, 100.0f },
    { "EMBERS",          3.0f,  0,   40.0f, -40.0f, 100.0f },
    { "WARM TUBE",       8.0f,  0,   20.0f, -20.0f, 100.0f },
    { "TAPE GLUE",       8.0f,  1,    0.0f, -30.0f, 100.0f },
    { "HOT TAPE",       18.0f,  1,   10.0f, -10.0f, 100.0f },
    { "PARALLEL HEAT",  24.0f,  0,   30.0f,  20.0f,  35.0f },
    { "DRUM CRUSH",     20.0f,  1,    0.0f,  40.0f,  60.0f },
    { "BASS BURN",      15.0f,  2,   20.0f, -60.0f,  50.0f },
    { "FUZZ BOX",       30.0f,  2,    0.0f,  30.0f, 100.0f },
    { "SPUTTER",        36.0f,  2,   75.0f,   0.0f, 100.0f },
    { "FOLDBACK",       16.0f,  3,    0.0f,   0.0f, 100.0f },
    { "WITCH HOWL",     30.0f,  3,   50.0f,  40.0f,  70.0f },
};
inline constexpr int count = (int) (sizeof (factory) / sizeof (factory[0]));

/** Parameter id -> value pairs of a preset (choices as index). Output, auto gain and bypass are left alone. */
inline std::array<std::pair<const char*, float>, 5> values (const Preset& p)
{
    return { { { params::drive, p.drive }, { params::mode, (float) p.mode }, { params::bias, p.bias },
               { params::tone, p.tone }, { params::mix, p.mix } } };
}
} // namespace presets
