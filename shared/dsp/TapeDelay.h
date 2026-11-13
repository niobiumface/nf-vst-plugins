#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>

namespace sdsp
{
/** Stereo tape delay (Séance).

    Per sample:  record = in + feedback * playback(crossed for ping-pong)
                 record -> highcut (12 dB/oct) -> lowcut (12 dB/oct) -> tape saturation -> tape
                 playback = tape read at (delay time + wow + flutter), cubic Hermite interpolation

    The filters and the saturation sit in the record path, so every repeat (the first one too) passes them once more:
    each echo is darker, thinner and a little more compressed than the one before. The saturation has slope 1 at 0
    and is bounded, so feedback above 100 % swells into self-oscillation instead of blowing up.

    Delay-time changes glide with a ~0.25 s one-pole like a tape motor changing speed (pitch bends, no clicks).
    Ping-pong crossfades smoothly: the input is summed into the left line and each line feeds the other. */
class TapeDelay
{
public:
    static constexpr float maxModMs = 6.0f;

    struct Params
    {
        float delaySamples = 18000.0f; // target, before modulation
        float feedback = 0.4f;         // 0 .. 1.1
        float wow = 0.2f;              // 0 .. 1 (wow and flutter depth together)
        float lowCutHz = 80.0f;
        float highCutHz = 6000.0f;
        float pingPong = 0.0f;         // 0 normal, 1 ping-pong (crossfaded)
    };

    void prepare (double sampleRate, double maxSeconds)
    {
        fs = (float) sampleRate;
        const int need = (int) std::ceil (maxSeconds * sampleRate + maxModMs * 0.001 * sampleRate) + 8;
        int size = 1;
        while (size < need) size <<= 1;
        mask = size - 1;
        for (auto& b : line) b.assign ((size_t) size, 0.0f);
        maxDelay = (float) (need - 8);
        glideCoef = std::exp (-1.0f / (0.25f * fs));
        paramCoef = std::exp (-1.0f / (0.03f * fs));
        wowInc = 0.55f / fs;
        flutInc = 6.8f / fs;
        flut2Inc = 11.3f / fs;
        driftCoef = std::exp (-1.0f / (0.6f * fs));
        reset();
    }

    void reset()
    {
        for (auto& b : line) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& s : lp) s = {};
        for (auto& s : hp) s = {};
        writePos = 0;
        delay = target.delaySamples;
        fbS = target.feedback; wowS = target.wow; ppS = target.pingPong;
        lowS = target.lowCutHz; highS = target.highCutHz;
        updateFilters();
        drift = driftTarget = 0.0f;
        driftCount = 0;
    }

    /** Call once per block (or whenever). Values are smoothed inside. */
    void setParams (const Params& p) noexcept
    {
        target = p;
        target.delaySamples = std::clamp (p.delaySamples, 1.0f, maxDelay);
        target.feedback = std::clamp (p.feedback, 0.0f, 1.2f);
        target.lowCutHz = std::clamp (p.lowCutHz, 10.0f, fs * 0.2f);
        target.highCutHz = std::clamp (p.highCutHz, 200.0f, fs * 0.45f);
    }

    /** Snap all smoothed values to their targets (after prepare/first setParams). */
    void snap() noexcept
    {
        delay = target.delaySamples;
        fbS = target.feedback; wowS = target.wow; ppS = target.pingPong;
        lowS = target.lowCutHz; highS = target.highCutHz;
        updateFilters();
    }

    /** Processes one stereo frame: records the dry input, returns the wet (playback) signal. */
    void process (float inL, float inR, float& wetL, float& wetR) noexcept
    {
        // --- smoothing
        delay = target.delaySamples + glideCoef * (delay - target.delaySamples);
        fbS   = target.feedback   + paramCoef * (fbS - target.feedback);
        wowS  = target.wow        + paramCoef * (wowS - target.wow);
        ppS   = target.pingPong   + paramCoef * (ppS - target.pingPong);
        lowS  = target.lowCutHz   + paramCoef * (lowS - target.lowCutHz);
        highS = target.highCutHz  + paramCoef * (highS - target.highCutHz);
        if (++filterCount >= 16) { filterCount = 0; updateFilters(); }

        // --- wow (slow sine + random drift) and flutter (two faster sines)
        wowPh += wowInc;   if (wowPh >= 1.0f) wowPh -= 1.0f;
        flutPh += flutInc; if (flutPh >= 1.0f) flutPh -= 1.0f;
        flut2Ph += flut2Inc; if (flut2Ph >= 1.0f) flut2Ph -= 1.0f;
        if (--driftCount <= 0)
        {
            driftCount = (int) (fs * 0.35f);
            driftTarget = rand11();
        }
        drift = driftTarget + driftCoef * (drift - driftTarget);

        constexpr float twoPi = 6.28318530718f;
        const float w = wowS * wowS; // depth curve: gentle at the bottom
        const float wowMs = w * 3.2f * (0.7f * std::sin (twoPi * wowPh) + 0.6f * drift);
        const float flutMs = w * 0.16f * (std::sin (twoPi * flutPh) + 0.35f * std::sin (twoPi * flut2Ph));
        const float mod = (wowMs + flutMs) * 0.001f * fs;
        // the right channel reads a hair later on the "tape" (a little width, same motor)
        const float flutR = w * 0.16f * std::sin (twoPi * (flutPh + 0.23f));
        const float modR = (wowMs + flutR) * 0.001f * fs;

        const float dL = std::clamp (delay + mod, 2.0f, maxDelay + maxModMs * 0.001f * fs);
        const float dR = std::clamp (delay + modR, 2.0f, maxDelay + maxModMs * 0.001f * fs);
        wetL = read (0, dL);
        wetR = read (1, dR);

        // --- record: input + crossed feedback
        const float x = ppS;
        const float recInL = inL * (1.0f - x) + 0.5f * (inL + inR) * x;
        const float recInR = inR * (1.0f - x);
        const float fbL = wetL * (1.0f - x) + wetR * x;
        const float fbR = wetR * (1.0f - x) + wetL * x;
        float recL = recInL + fbS * fbL;
        float recR = recInR + fbS * fbR;

        recL = tape (hp[0].process (lp[0].process (recL, lpC), hpC));
        recR = tape (hp[1].process (lp[1].process (recR, lpC), hpC));

        line[0][(size_t) writePos] = recL;
        line[1][(size_t) writePos] = recR;
        writePos = (writePos + 1) & mask;
    }

    float getCurrentDelaySamples() const noexcept { return delay; }
    float getMaxDelaySamples() const noexcept { return maxDelay; }

    /** Tape saturation: tanh(k x) / k - slope 1 at 0, bounded at 1/k. */
    static float tape (float x) noexcept
    {
        constexpr float k = 0.6f;
        return std::tanh (k * x) * (1.0f / k);
    }

private:
    /** TPT state-variable filter section (12 dB/oct, Butterworth Q). */
    struct Coef { float g = 0.0f, k = 1.41421356f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f; };
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        float run (float v0, const Coef& c, float& bp) noexcept
        {
            const float v3 = v0 - ic2;
            const float v1 = c.a1 * ic1 + c.a2 * v3;
            const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            bp = v1;
            return v2; // lowpass
        }
    };
    struct Lowpass  { Svf s; float process (float x, const Coef& c) noexcept { float bp; return s.run (x, c, bp); } };
    struct Highpass { Svf s; float process (float x, const Coef& c) noexcept { float bp; const float lo = s.run (x, c, bp); return x - c.k * bp - lo; } };

    static Coef makeCoef (float hz, float fs) noexcept
    {
        Coef c;
        c.g = std::tan (3.14159265f * hz / fs);
        c.a1 = 1.0f / (1.0f + c.g * (c.g + c.k));
        c.a2 = c.g * c.a1;
        c.a3 = c.g * c.a2;
        return c;
    }

    void updateFilters() noexcept
    {
        lpC = makeCoef (highS, fs);
        hpC = makeCoef (lowS, fs);
    }

    /** Cubic Hermite read `d` samples behind the write head (the sample written last is 1 behind). */
    float read (int ch, float d) const noexcept
    {
        const float pos = (float) writePos - d;
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const int i = (int) fl;
        const auto& b = line[(size_t) ch];
        const float ym1 = b[(size_t) ((i - 1) & mask)], y0 = b[(size_t) (i & mask)];
        const float y1 = b[(size_t) ((i + 1) & mask)], y2 = b[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    float rand11() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    float fs = 48000.0f;
    std::vector<float> line[2];
    int mask = 0, writePos = 0;
    float maxDelay = 1.0f;

    Params target;
    float delay = 0.0f, fbS = 0.0f, wowS = 0.0f, ppS = 0.0f, lowS = 80.0f, highS = 6000.0f;
    float glideCoef = 0.0f, paramCoef = 0.0f;
    int filterCount = 0;
    Coef lpC, hpC;
    Lowpass lp[2];
    Highpass hp[2];

    float wowPh = 0.0f, flutPh = 0.0f, flut2Ph = 0.0f, wowInc = 0.0f, flutInc = 0.0f, flut2Inc = 0.0f;
    float drift = 0.0f, driftTarget = 0.0f, driftCoef = 0.0f;
    int driftCount = 0;
    uint32_t rng = 0x1234567u;
};

/** Onset detector for one channel: fires when a fast envelope jumps well above a slow one
    (used to tell the UI when an echo arrives). */
class OnsetDetector
{
public:
    void prepare (double sampleRate) noexcept
    {
        const float fs = (float) sampleRate;
        fastAtt = std::exp (-1.0f / (0.0005f * fs));
        fastRel = std::exp (-1.0f / (0.015f * fs));
        slowAtt = std::exp (-1.0f / (0.040f * fs));
        slowRel = std::exp (-1.0f / (0.200f * fs));
        refractory = (int) (0.06f * fs);
        reset();
    }

    void reset() noexcept { fast = slow = 0.0f; armed = true; wait = 0; }

    /** Returns the onset level (> 0) when an onset happens on this sample, else 0. */
    float process (float x) noexcept
    {
        const float a = std::abs (x);
        fast = a + (a > fast ? fastAtt : fastRel) * (fast - a);
        slow = a + (a > slow ? slowAtt : slowRel) * (slow - a);
        if (wait > 0) --wait;
        if (! armed && fast < slow * 1.25f) armed = true;
        if (armed && wait == 0 && fast > floor && fast > slow * 2.0f + floor)
        {
            armed = false;
            wait = refractory;
            return fast;
        }
        return 0.0f;
    }

    float floor = 0.002f; // ~ -54 dBFS

private:
    float fast = 0.0f, slow = 0.0f, fastAtt = 0.0f, fastRel = 0.0f, slowAtt = 0.0f, slowRel = 0.0f;
    bool armed = true;
    int wait = 0, refractory = 0;
};
} // namespace sdsp
