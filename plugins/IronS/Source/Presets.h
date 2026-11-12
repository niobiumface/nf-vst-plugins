#pragma once
#include <array>
#include "Params.h"

namespace presets
{
struct Preset
{
    const char* name;
    float thresh, range, freq, width, speed, smack, look;
    bool absolute, high, split;
};

// Factory presets. Order is permanent once released (hosts store the index).
inline constexpr Preset factory[] = {
    //  name               thr     rng    freq     wid   spd    smk    look  abs    high   split
    { "INIT",            -14.0f,  9.0f,  6500.0f, 1.2f,  40.0f,  0.0f, 2.0f, false, false, true  },
    { "FEMALE VOX",      -12.0f,  8.0f,  7500.0f, 1.2f,  35.0f,  0.0f, 2.0f, false, false, true  },
    { "MALE VOX",        -12.0f,  8.0f,  5500.0f, 1.4f,  45.0f,  0.0f, 2.0f, false, false, true  },
    { "BRIGHT POP VOX",  -10.0f,  6.0f,  8500.0f, 1.0f,  30.0f, 10.0f, 2.0f, false, false, true  },
    { "PODCAST",         -14.0f, 10.0f,  6000.0f, 1.6f,  50.0f, 35.0f, 3.0f, false, false, true  },
    { "SMACK KILLER",     -8.0f,  6.0f,  6500.0f, 1.5f,  40.0f, 85.0f, 3.0f, false, false, true  },
    { "WHISPER",         -16.0f, 10.0f,  7000.0f, 1.6f,  50.0f, 60.0f, 3.0f, false, false, true  },
    { "HEAVY LISP",      -18.0f, 16.0f,  5000.0f, 1.2f,  60.0f, 20.0f, 3.0f, false, true,  true  },
    { "GENTLE WIDE",     -10.0f,  5.0f,  6500.0f, 1.5f,  60.0f,  0.0f, 2.0f, false, false, false },
    { "CYMBAL TAME",     -24.0f,  6.0f,  9000.0f, 1.2f,  80.0f,  0.0f, 1.0f, true,  true,  true  },
    { "GUITAR FIZZ",     -30.0f,  8.0f,  4500.0f, 1.0f,  70.0f,  0.0f, 1.0f, true,  false, true  },
    { "MASTER DE-HARSH", -28.0f,  3.0f,  5000.0f, 2.0f, 100.0f,  0.0f, 3.0f, true,  false, true  },
};
inline constexpr int count = (int) (sizeof (factory) / sizeof (factory[0]));

/** Parameter id -> value pairs of a preset (choices as index). Output, link, listen and bypass are left alone. */
inline std::array<std::pair<const char*, float>, 10> values (const Preset& p)
{
    return { { { params::thresh, p.thresh }, { params::range, p.range }, { params::freq, p.freq }, { params::width, p.width },
               { params::speed, p.speed }, { params::smack, p.smack }, { params::look, p.look },
               { params::mode, p.absolute ? 1.0f : 0.0f }, { params::detect, p.high ? 1.0f : 0.0f },
               { params::cut, p.split ? 1.0f : 0.0f } } };
}
} // namespace presets
