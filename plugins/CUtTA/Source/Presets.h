#pragma once
#include "Params.h"

namespace presets
{
struct Preset
{
    const char* name;
    bool gateOn; float thresh, range, attack, hold, release;
    bool limOn;  float drive, ceiling, limRelease;
};

// Factory presets. Order is permanent once released (hosts store the index).
inline constexpr Preset factory[] = {
    //  name              gate  thr    rng    atk    hold   rel      lim   drv    ceil   rel
    { "INIT",            true, -40.0f, -80.0f, 0.5f,  40.0f, 120.0f, true,  0.0f, -0.3f,  50.0f },
    { "KICK TIGHT",      true, -30.0f, -80.0f, 0.1f,  20.0f,  60.0f, true,  2.0f, -0.3f,  30.0f },
    { "SNARE CLEAN",     true, -28.0f, -60.0f, 0.2f,  30.0f,  90.0f, true,  1.0f, -0.5f,  40.0f },
    { "TOMS",            true, -35.0f, -70.0f, 0.3f,  80.0f, 250.0f, true,  0.0f, -0.5f,  50.0f },
    { "GUITAR CHUG",     true, -45.0f, -80.0f, 0.3f,  10.0f,  40.0f, true,  0.0f, -0.5f,  50.0f },
    { "BASS DI",         true, -55.0f, -40.0f, 2.0f,  50.0f, 150.0f, true,  3.0f, -0.5f,  60.0f },
    { "VOCAL CLEANUP",   true, -50.0f, -20.0f, 1.0f,  60.0f, 200.0f, true,  2.0f, -1.0f,  80.0f },
    { "PODCAST",         true, -48.0f, -15.0f, 3.0f, 120.0f, 300.0f, true,  4.0f, -1.0f, 100.0f },
    { "SOFT EXPANDER",   true, -45.0f, -12.0f, 5.0f, 100.0f, 400.0f, false, 0.0f, -0.3f,  50.0f },
    { "GATED VERB",      true, -30.0f, -80.0f, 0.1f, 180.0f,  15.0f, true,  0.0f, -0.3f,  50.0f },
    { "BRICKWALL",       false,-40.0f, -80.0f, 0.5f,  40.0f, 120.0f, true,  6.0f, -0.1f,  20.0f },
    { "CRUSHER",         true, -35.0f, -80.0f, 0.05f,  0.0f,  15.0f, true, 18.0f, -0.3f,   5.0f },
};
inline constexpr int count = (int) (sizeof (factory) / sizeof (factory[0]));

/** Parameter id -> value pairs of a preset (bools as 0/1). */
inline std::array<std::pair<const char*, float>, 10> values (const Preset& p)
{
    return { { { params::gateOn, p.gateOn ? 1.0f : 0.0f }, { params::gateThresh, p.thresh }, { params::gateRange, p.range },
               { params::gateAttack, p.attack }, { params::gateHold, p.hold }, { params::gateRelease, p.release },
               { params::limOn, p.limOn ? 1.0f : 0.0f }, { params::limDrive, p.drive }, { params::limCeiling, p.ceiling },
               { params::limRelease, p.limRelease } } };
}
} // namespace presets
