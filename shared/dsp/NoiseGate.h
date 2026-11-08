#pragma once
#include <cmath>
#include <algorithm>

namespace sdsp
{
inline float dbToGain (float db) noexcept { return db <= -100.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  noexcept { return g <= 1.0e-5f ? -100.0f : 20.0f * std::log10 (g); }
inline float onePoleCoef (float seconds, double fs) noexcept
{
    return seconds <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (seconds * fs));
}

/** Stereo-linked noise gate: peak detector, hysteresis, hold, attack/release gain smoothing, range. */
class NoiseGate
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        detRelease = onePoleCoef (0.020f, fs);
        reset();
    }

    void reset() noexcept { env = 0.0f; gain = 0.0f; holdLeft = 0; open = false; }

    void setParams (float thresholdDb, float rangeDb, float attackMs, float holdMs, float releaseMs) noexcept
    {
        openThr   = dbToGain (thresholdDb);
        closeThr  = dbToGain (thresholdDb - hysteresisDb);
        floorGain = dbToGain (rangeDb);
        attCoef   = onePoleCoef (attackMs * 0.001f, fs);
        relCoef   = onePoleCoef (releaseMs * 0.001f, fs);
        holdSamples = (int) std::round (holdMs * 0.001 * fs);
    }

    /** Feed the linked detector value (max |x| over channels), returns gain for this sample. */
    float process (float detector) noexcept
    {
        env = detector > env ? detector : detector + detRelease * (env - detector);

        if (env >= openThr)            { open = true; holdLeft = holdSamples; }
        else if (env < closeThr && open)
        {
            if (holdLeft > 0) --holdLeft;
            else              open = false;
        }

        const float target = open ? 1.0f : floorGain;
        const float c = target > gain ? attCoef : relCoef;
        gain = target + c * (gain - target);
        return gain;
    }

    bool isOpen() const noexcept { return open; }
    float getGain() const noexcept { return gain; }

private:
    static constexpr float hysteresisDb = 4.0f;
    double fs = 44100.0;
    float env = 0.0f, gain = 0.0f;
    float openThr = 0.01f, closeThr = 0.006f, floorGain = 0.0f;
    float attCoef = 0.0f, relCoef = 0.0f, detRelease = 0.0f;
    int holdSamples = 0, holdLeft = 0;
    bool open = false;
};
} // namespace sdsp
