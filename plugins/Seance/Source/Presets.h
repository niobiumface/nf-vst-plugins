#pragma once
#include <array>
#include "Params.h"

namespace presets
{
struct Preset
{
    const char* name;
    float time;     // ms (used when not synced)
    bool sync;
    int division;   // index into params::divisionNames
    float feedback, wow, lowCut, highCut;
    bool pingPong;
    float mix;
};

// Factory presets. Order is permanent once released (hosts store the index).
inline constexpr Preset factory[] = {
    //  name               time   sync  div  fdbk    wow   lowcut  highcut  ping   mix
    { "INIT",             375.0f, false, 7,  45.0f, 25.0f, 120.0f,  5500.0f, false, 35.0f },
    { "SLAPBACK",          95.0f, false, 7,  10.0f, 15.0f, 150.0f,  4500.0f, false, 30.0f },
    { "DOTTED EIGHTHS",   375.0f, true,  7,  40.0f, 12.0f, 200.0f,  7000.0f, false, 30.0f },
    { "WHISPERING WALLS", 250.0f, true,  6,  55.0f, 20.0f, 300.0f,  3500.0f, true,  30.0f },
    { "PING PONG GHOSTS", 375.0f, true,  7,  60.0f, 18.0f, 180.0f,  6000.0f, true,  35.0f },
    { "DUB SPIRITS",      500.0f, true,  9,  75.0f, 30.0f, 350.0f,  2500.0f, true,  40.0f },
    { "WARPED TAPE",      420.0f, false, 7,  50.0f, 85.0f, 100.0f,  4000.0f, false, 40.0f },
    { "CELLAR ECHO",      180.0f, false, 7,  65.0f, 25.0f,  60.0f,  1800.0f, false, 30.0f },
    { "THIN AIR",         320.0f, false, 7,  60.0f, 20.0f, 900.0f, 12000.0f, true,  30.0f },
    { "LONG GOODBYE",     900.0f, false, 7,  82.0f, 30.0f, 150.0f,  3000.0f, true,  35.0f },
    { "VOICES",            40.0f, false, 7,  70.0f, 60.0f, 250.0f,  6000.0f, true,  45.0f },
    { "POSSESSED",        280.0f, false, 7, 104.0f, 45.0f, 200.0f,  3200.0f, true,  40.0f },
};
inline constexpr int count = (int) (sizeof (factory) / sizeof (factory[0]));

/** Parameter id -> value pairs of a preset (bools/choices as 0/1/index). Bypass is left alone. */
inline std::array<std::pair<const char*, float>, 9> values (const Preset& p)
{
    return { { { params::time, p.time }, { params::sync, p.sync ? 1.0f : 0.0f }, { params::division, (float) p.division },
               { params::feedback, p.feedback }, { params::wow, p.wow }, { params::lowCut, p.lowCut },
               { params::highCut, p.highCut }, { params::pingPong, p.pingPong ? 1.0f : 0.0f }, { params::mix, p.mix } } };
}
} // namespace presets
