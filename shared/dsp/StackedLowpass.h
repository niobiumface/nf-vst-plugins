#pragma once
#include <cmath>
#include <algorithm>

namespace sdsp
{
/** Zero-delay-feedback (TPT) state variable filter, lowpass output only. */
struct SvfLowpass
{
    float ic1 = 0.0f, ic2 = 0.0f;

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    /** a1/a2/a3 from SvfCoefs. */
    float process (float x, float a1, float a2, float a3) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
};

struct SvfCoefs
{
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;

    void set (float g, float q) noexcept
    {
        const float k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

/** Up to 4 cascaded 12 dB/oct lowpass "layers" (12..48 dB/oct).
    All layers always run, the output morphs between the tap after layer N and N+1, so the
    layer count can be smoothed without clicks. Resonance sits on the first layer. */
class StackedLowpass
{
public:
    static constexpr int maxLayers = 4, maxChannels = 2;
    static constexpr float flatQ = 0.7071f, maxQ = 16.0f;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        for (auto& ch : svf)
            for (auto& s : ch) s.reset();
        lastG = -1.0f; lastQ = -1.0f;
    }

    static float resoToQ (float reso01) noexcept
    {
        return flatQ + reso01 * reso01 * (maxQ - flatQ);
    }

    /** Input gain that keeps resonant peaks roughly in check. */
    static float resoCompensation (float reso01) noexcept { return 1.0f / (1.0f + 1.5f * reso01 * reso01); }

    /** Call per sample (cheap when values don't change). */
    void setParams (float cutoffHz, float reso01) noexcept
    {
        const float fc = std::clamp (cutoffHz, 10.0f, (float) (fs * 0.45));
        const float g = (float) std::tan (3.14159265358979 * fc / fs);
        const float q = resoToQ (reso01);
        if (g != lastG || q != lastQ)
        {
            first.set (g, q);
            rest.set (g, flatQ);
            lastG = g; lastQ = q;
        }
        comp = resoCompensation (reso01);
    }

    /** layerPos in [1, maxLayers], fractional values crossfade between taps. */
    float process (int ch, float x, float layerPos) noexcept
    {
        auto& s = svf[ch];
        float taps[maxLayers];
        float y = s[0].process (x * comp, first.a1, first.a2, first.a3);
        taps[0] = y;
        for (int i = 1; i < maxLayers; ++i)
            taps[i] = y = s[i].process (y, rest.a1, rest.a2, rest.a3);

        const float p = std::clamp (layerPos, 1.0f, (float) maxLayers) - 1.0f;
        const int idx = std::min ((int) p, maxLayers - 2);
        const float frac = p - (float) idx;
        return taps[idx] + (taps[idx + 1] - taps[idx]) * frac;
    }

    /** Analog-prototype magnitude at frequency f, for display (not real-time critical). */
    static float magnitude (float f, float cutoffHz, float reso01, float layerPos) noexcept
    {
        auto stage = [] (float r, float q)
        {
            const float a = 1.0f - r * r, b = r / q;
            return 1.0f / std::sqrt (a * a + b * b);
        };
        const float r = f / cutoffHz;
        float taps[maxLayers];
        float m = stage (r, resoToQ (reso01)) * resoCompensation (reso01);
        taps[0] = m;
        const float flat = stage (r, flatQ);
        for (int i = 1; i < maxLayers; ++i) taps[i] = m *= flat;

        const float p = std::clamp (layerPos, 1.0f, (float) maxLayers) - 1.0f;
        const int idx = std::min ((int) p, maxLayers - 2);
        const float frac = p - (float) idx;
        return taps[idx] + (taps[idx + 1] - taps[idx]) * frac;
    }

private:
    double fs = 44100.0;
    SvfLowpass svf[maxChannels][maxLayers];
    SvfCoefs first, rest;
    float lastG = -1.0f, lastQ = -1.0f, comp = 1.0f;
};

/** Soft saturation with a drive in dB; 0 dB is a clean bypass, morphs in over the first 3 dB. */
inline float driveSaturate (float x, float driveGain, float wet) noexcept
{
    if (wet <= 0.0f) return x;
    const float sat = std::tanh (x * driveGain) / std::tanh (std::max (driveGain, 1.0f));
    return x + (sat - x) * wet;
}
} // namespace sdsp
