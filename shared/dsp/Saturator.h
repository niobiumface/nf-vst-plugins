#pragma once
#include <cmath>
#include <algorithm>
#include <array>

namespace sdsp
{
/** Waveshaping saturator (Pyre). Runs at the oversampled rate - the caller up/downsamples.

    Signal path per sample (wet):  x * drive + bias -> shaper -> minus shaper(bias) -> DC blocker -> tilt tone -> auto gain
    and then the dry/wet mix, also in the oversampled domain so dry and wet share the same filters (no comb effects).

    All shapers have slope 1 at the origin, so at 0 dB drive quiet signals pass unchanged. */
class Saturator
{
public:
    enum Mode { tube = 0, tape, fuzz, fold, numModes };
    static constexpr int maxChannels = 2;
    static constexpr float maxBias = 0.6f;     // bias at 100 % in shaper units
    static constexpr float tiltLowDb = 4.0f, tiltHighDb = 8.0f;

    /** The raw curves (slope 1 at 0, output roughly within [-1, 1]). */
    static float shape (int mode, float u) noexcept
    {
        switch (mode)
        {
            case tube: // asymmetric: the negative half compresses earlier -> even harmonics even without bias
                return u >= 0.0f ? std::tanh (u) : std::tanh (1.25f * u) * 0.8f;
            case tape: // soft cubic, flat at |u| >= 1.5
            {
                const float a = std::clamp (u, -1.5f, 1.5f);
                return a - a * a * a * (4.0f / 27.0f);
            }
            case fuzz: // nearly hard: u / (1 + u^8)^(1/8)
            {
                const float a2 = u * u, a4 = a2 * a2;
                return u / std::sqrt (std::sqrt (std::sqrt (1.0f + a4 * a4)));
            }
            case fold: // sine wavefolder: rises to 1, then folds back over and over
            default:
                return std::sin (u);
        }
    }

    /** Shaper with bias, DC-free at rest: f(u + b) - f(b). */
    static float shapeBiased (int mode, float u, float b) noexcept { return shape (mode, u + b) - shape (mode, b); }

    /** Auto gain: makes a sine of `ref` peak come out with the same RMS it went in with. */
    static float autoGain (int mode, float driveGain, float bias) noexcept
    {
        constexpr float ref = 0.25f;
        constexpr int n = 128; // enough points for a heavily folded sine
        static const auto sine = []
        {
            std::array<float, n> t {};
            for (int i = 0; i < n; ++i) t[(size_t) i] = std::sin (6.2831853f * ((float) i + 0.5f) / (float) n);
            return t;
        }();
        float sum = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float s = ref * sine[(size_t) i];
            const float y = shapeBiased (mode, s * driveGain, bias);
            sum += y * y;
        }
        const float rmsOut = std::sqrt (sum / (float) n);
        const float rmsIn = ref * 0.70710678f;
        return std::clamp (rmsIn / std::max (rmsOut, 1.0e-6f), 1.0f / 16.0f, 4.0f);
    }

    void prepare (double oversampledRate) noexcept
    {
        fs = oversampledRate;
        dcR   = (float) std::exp (-6.283185307 * 8.0 / fs);
        toneC = (float) std::exp (-6.283185307 * 700.0 / fs);
        xfStep = (float) (1.0 / (0.025 * fs));
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : st) s = {};
        xfade = 0.0f;
        prevMode = curMode;
    }

    /** Mode changes crossfade over 25 ms. */
    void setMode (int m) noexcept
    {
        m = std::clamp (m, 0, numModes - 1);
        if (m == curMode) return;
        prevMode = curMode;
        curMode = m;
        xfade = 1.0f;
    }
    int getMode() const noexcept { return curMode; }

    /** Call once per oversampled frame before process() (advances the mode crossfade). */
    void tick() noexcept { xfade = std::max (0.0f, xfade - xfStep); }

    /** Tilt gains for tone -1..1: -4..+4 dB below / -8..+8 dB above ~700 Hz, exactly flat at 0. */
    static float tiltLowGain (float tone) noexcept  { return dbToGain (-tone * tiltLowDb); }
    static float tiltHighGain (float tone) noexcept { return dbToGain (tone * tiltHighDb); }

    /** One oversampled sample. drive = linear gain, bias in shaper units, lowG/highG from tiltLowGain/tiltHighGain,
        comp = auto gain, mix 0..1. */
    float process (int ch, float x, float drive, float bias, float lowG, float highG, float comp, float mix) noexcept
    {
        auto& s = st[ch];
        const float u = x * drive;
        float y = shapeBiased (curMode, u, bias);
        if (xfade > 0.0f) y += (shapeBiased (prevMode, u, bias) - y) * xfade;

        // DC blocker (the bias and the asymmetric curves create DC)
        const float hp = y - s.dcX + dcR * s.dcY;
        s.dcX = y; s.dcY = hp;

        // tilt around ~700 Hz
        s.lp += (1.0f - toneC) * (hp - s.lp);
        const float w = (s.lp * lowG + (hp - s.lp) * highG) * comp;

        return x + (w - x) * mix;
    }

    static float dbToGain (float db) noexcept { return std::exp (db * 0.11512925f); }

private:
    struct State { float dcX = 0.0f, dcY = 0.0f, lp = 0.0f; };
    State st[maxChannels];
    double fs = 176400.0;
    float dcR = 0.9997f, toneC = 0.975f, xfStep = 0.0002f, xfade = 0.0f;
    int curMode = tube, prevMode = tube;
};
} // namespace sdsp
