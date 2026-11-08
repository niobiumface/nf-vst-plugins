#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "NoiseGate.h" // dbToGain / onePoleCoef

namespace sdsp
{
/** Stereo-linked lookahead brickwall limiter (min-hold + moving-average attack, one-pole release).
    Latency = getLatency() samples. All memory is allocated in prepare(). */
class LookaheadLimiter
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, float lookaheadMs = 1.5f)
    {
        fs = sampleRate;
        L = std::max (1, (int) std::round (lookaheadMs * 0.001 * fs));
        size = L + 1;
        for (auto& d : delay) d.assign ((size_t) size, 0.0f);
        reqRing.assign ((size_t) size, 1.0f);
        dqIdx.assign ((size_t) size + 1, 0);
        dqVal.assign ((size_t) size + 1, 1.0f);
        avgRing.assign ((size_t) L, 1.0f);
        reset();
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (avgRing.begin(), avgRing.end(), 1.0f);
        writePos = 0; avgPos = 0; avgSum = (double) L;
        dqHead = dqTail = 0; counter = 0;
        smooth = 1.0f; gain = 1.0f;
    }

    int getLatency() const noexcept { return L; }

    void setParams (float ceilingDb, float releaseMs) noexcept
    {
        ceiling = dbToGain (ceilingDb);
        relCoef = onePoleCoef (releaseMs * 0.001f, fs);
    }

    /** Processes one frame in place (numCh <= 2). If !active the signal is only delayed. Returns gain applied. */
    float processFrame (float* frame, int numCh, bool active) noexcept
    {
        float peak = 0.0f;
        for (int c = 0; c < numCh; ++c) peak = std::max (peak, std::abs (frame[c]));

        const float req = (active && peak > ceiling) ? ceiling / peak : 1.0f;

        // sliding-window minimum over the last L+1 required gains (monotonic deque)
        const long long n = counter++;
        while (dqTail > dqHead && dqVal[(size_t) ((dqTail - 1) % (long long) dqVal.size())] >= req) --dqTail;
        dqVal[(size_t) (dqTail % (long long) dqVal.size())] = req;
        dqIdx[(size_t) (dqTail % (long long) dqIdx.size())] = n;
        ++dqTail;
        while (dqIdx[(size_t) (dqHead % (long long) dqIdx.size())] <= n - size) ++dqHead;
        const float m = dqVal[(size_t) (dqHead % (long long) dqVal.size())];

        // instant attack onto held minimum, smooth release
        smooth = m < smooth ? m : m + relCoef * (smooth - m);

        // moving average over L samples -> attack ramp
        avgSum += (double) smooth - (double) avgRing[(size_t) avgPos];
        avgRing[(size_t) avgPos] = smooth;
        avgPos = (avgPos + 1) % L;
        gain = std::min (1.0f, (float) (avgSum / (double) L));

        // delay line
        const int readPos = (writePos + 1) % size; // oldest = L samples back
        for (int c = 0; c < numCh; ++c)
        {
            auto& d = delay[(size_t) c];
            d[(size_t) writePos] = frame[c];
            float y = d[(size_t) readPos];
            if (active) y = std::clamp (y * gain, -ceiling, ceiling);
            frame[c] = y;
        }
        writePos = readPos;
        return active ? gain : 1.0f;
    }

private:
    double fs = 44100.0;
    int L = 64, size = 65;
    std::vector<float> delay[maxChannels];
    std::vector<float> reqRing, dqVal, avgRing;
    std::vector<long long> dqIdx;
    long long dqHead = 0, dqTail = 0, counter = 0;
    int writePos = 0, avgPos = 0;
    double avgSum = 0.0;
    float ceiling = 1.0f, relCoef = 0.0f, smooth = 1.0f, gain = 1.0f;
};
} // namespace sdsp
