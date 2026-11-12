#pragma once
#include <cmath>
#include <algorithm>
#include <vector>

namespace sdsp
{
/** Coefficients of a zero-delay-feedback (TPT) state variable filter. */
struct TptCoef
{
    float k = 1.41421f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;

    void set (float g, float q) noexcept
    {
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

/** TPT state variable filter with simultaneous lowpass / bandpass / highpass outputs. */
struct TptSvf
{
    float ic1 = 0.0f, ic2 = 0.0f, lp = 0.0f, bp = 0.0f, hp = 0.0f;

    void reset() noexcept { ic1 = ic2 = lp = bp = hp = 0.0f; }

    void process (float x, const TptCoef& c) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - c.k * v1 - v2;
    }
};

/** Dynamic 12 dB/oct high shelf: unity below the corner, `gain` above it.
    Built from one SVF as lp + k*sqrt(gain)*bp + gain*hp. With gain = 1 this is lp + k*bp + hp = the input
    itself, so the shelf is completely transparent (no crossover phase shift) while nothing is reduced.
    The poles never move, only the output mix does, so the gain can change every sample without clicks. */
struct DynamicShelf
{
    TptSvf svf;

    void reset() noexcept { svf.reset(); }

    float process (float x, float gain, const TptCoef& c) noexcept
    {
        svf.process (x, c);
        return svf.lp + c.k * std::sqrt (gain) * svf.bp + gain * svf.hp;
    }
};

struct DeEsserParams
{
    float threshDb  = -14.0f;  // detector threshold (relative: band vs. full level, absolute: band level in dBFS)
    float rangeDb   = 9.0f;    // maximum reduction, positive dB
    float freqHz    = 6500.0f; // detection centre (band) or corner (high)
    float widthOct  = 1.2f;    // detection bandwidth in octaves (band detection only)
    float releaseMs = 40.0f;
    float lookMs    = 2.0f;    // 0 .. DeEsser::maxLookMs
    float smack     = 0.0f;    // 0..1 click / lip-smack suppression amount
    float link      = 1.0f;    // 0 = dual mono, 1 = fully stereo-linked
    bool  relative  = true;    // level-independent detection
    bool  bandDetect = true;   // false = highpass detection
    bool  split     = true;    // false = wideband reduction
    int   listen    = 0;       // 0 = output, 1 = detector sidechain, 2 = delta (what is removed)
};

/** De-esser with a click ("smack") suppressor in front.

    Detector (on the live input): two cascaded bandpass or highpass SVFs -> mean-square follower ->
    level in dB, either absolute or relative to the full-band level (then it works at any input gain).
    Gain computer: soft knee above the threshold, saturating smoothly at `range`.
    The audio runs through a fixed delay (maxLookMs, reported as latency); the gain is delayed a bit less,
    which gives the lookahead without ever changing the latency.
    Reduction is applied wideband or only to the highs through a dynamic high shelf (split), which is
    bit-transparent whenever nothing is reduced.

    Smack stage: crest factor (fast vs. slow mean square) of the signal above 1.5 kHz. An onset with a high
    crest ducks everything above 1.5 kHz (dynamic shelf) for a few milliseconds (5 ms hold, 7 ms fade) - enough to swallow a
    lip smack or click, while a sustained sound only loses the very start of its attack. */
class DeEsser
{
public:
    static constexpr int maxChannels = 2;
    static constexpr float maxLookMs = 5.0f;
    static constexpr float smackSplitHz = 1500.0f;
    static constexpr float floorDb = -60.0f;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        delayLen = std::max (1, (int) std::lround (maxLookMs * 0.001 * fs));
        for (int c = 0; c < maxChannels; ++c)
        {
            audioDelay[c].assign ((size_t) delayLen, 0.0f);
            redDelay[c].assign ((size_t) delayLen + 1, 0.0f);
        }
        smackDelay.assign ((size_t) delayLen + 1, 0.0f);

        envC      = coef (0.003f);
        fastC     = coef (0.0005f);
        slowC     = coef (0.120f);
        slowRelC  = coef (0.030f);
        smackAtkC = coef (0.0002f);
        smackRelC = coef (0.015f);
        mixC      = coef (0.010f);
        paramC    = 1.0f - std::exp (-(float) coefInterval / (0.02f * (float) fs));
        smackHold = std::max (1, (int) std::lround (0.005 * fs));
        smackFade = std::max (1, (int) std::lround (0.007 * fs));
        smackLook = std::min (delayLen, (int) std::lround (0.001 * fs));
        smackCoef.set ((float) std::tan (3.14159265358979 * smackSplitHz / fs), 0.70711f);

        setParams (p);
        reset();
    }

    void reset() noexcept
    {
        for (int c = 0; c < maxChannels; ++c)
        {
            std::fill (audioDelay[c].begin(), audioDelay[c].end(), 0.0f);
            std::fill (redDelay[c].begin(), redDelay[c].end(), 0.0f);
            scA[c].reset(); scB[c].reset(); smackHp[c].reset(); smackShelf[c].reset(); shelf[c].reset();
            scMs[c] = fullMs[c] = red[c] = 0.0f;
        }
        std::fill (smackDelay.begin(), smackDelay.end(), 0.0f);
        fastMs = slowMs = smackRed = 0.0f;
        smackArmed = true; smackAge = smackHold + smackFade;
        audioPos = gainPos = 0;
        tick = 0;
        logFreq = std::log2 (p.freqHz); width = p.widthOct;
        lastLogFreq = -1.0f;
        splitMix = p.split ? 1.0f : 0.0f;
        resetMeters();
    }

    int getLatency() const noexcept { return delayLen; }

    /** Call once per block. */
    void setParams (const DeEsserParams& np) noexcept
    {
        p = np;
        p.rangeDb = std::max (0.0f, p.rangeDb);
        p.smack = std::clamp (p.smack, 0.0f, 1.0f);
        p.link = std::clamp (p.link, 0.0f, 1.0f);
        const float lookSec = std::clamp (p.lookMs, 0.0f, maxLookMs) * 0.001f;
        look = std::min (delayLen, (int) std::lround (lookSec * (float) fs));
        atkC = coef (0.00015f + 0.3f * lookSec);
        relC = coef (std::max (1.0f, p.releaseMs) * 0.001f);
    }

    /** One frame. `in`, `out` and `dry` hold numCh samples; `dry` receives the latency-delayed input.
        `in` and `out` may be the same array. */
    void processFrame (const float* in, float* out, float* dry, int numCh) noexcept
    {
        if ((tick++ % coefInterval) == 0) updateCoefs();

        // --- detection on the live input
        float target[maxChannels] = {}, sc[maxChannels] = {};
        float smackIn = 0.0f, level = floorDb;
        for (int c = 0; c < numCh; ++c)
        {
            const float x = in[c];
            scA[c].process (x, scCoef);
            float s = p.bandDetect ? scA[c].bp * scCoef.k : scA[c].hp;
            scB[c].process (s, scCoef);
            s = p.bandDetect ? scB[c].bp * scCoef.k : scB[c].hp;
            sc[c] = s;

            scMs[c]   = s * s + envC * (scMs[c] - s * s);
            fullMs[c] = x * x + envC * (fullMs[c] - x * x);

            const float absDb = 10.0f * std::log10 (scMs[c] + 1.0e-12f);
            float l;
            if (p.relative)
            {
                // band level relative to the whole signal; faded out near silence so noise does not trigger
                l = 10.0f * std::log10 ((scMs[c] + 1.0e-12f) / (fullMs[c] + 1.0e-12f));
                const float gate = std::clamp ((absDb + 66.0f) / 12.0f, 0.0f, 1.0f);
                l = floorDb + (std::clamp (l, floorDb, 6.0f) - floorDb) * gate;
            }
            else
                l = std::clamp (absDb, floorDb, 6.0f);
            level = std::max (level, l);

            // soft knee (6 dB), then saturate smoothly at the range
            const float over = l - p.threshDb;
            float amount = over <= -3.0f ? 0.0f : (over < 3.0f ? (over + 3.0f) * (over + 3.0f) / 12.0f : over);
            if (p.relative) amount *= 1.5f;
            target[c] = p.rangeDb > 0.01f ? p.rangeDb * std::tanh (amount / p.rangeDb) : 0.0f;

            smackHp[c].process (x, smackCoef);
            smackIn = std::max (smackIn, smackHp[c].hp * smackHp[c].hp);
        }

        if (numCh > 1)
        {
            const float m = std::max (target[0], target[1]);
            target[0] += (m - target[0]) * p.link;
            target[1] += (m - target[1]) * p.link;
        }

        // --- click detector: crest factor of the highs
        fastMs = smackIn + fastC * (fastMs - smackIn);
        slowMs = fastMs + (fastMs > slowMs ? slowC : slowRelC) * (slowMs - fastMs); // slow up, quicker down
        float smackTarget = 0.0f;
        if (p.smack > 0.0f)
        {
            // an onset opens a short window (hold, then fade): whatever is still there afterwards is no click
            const float crest = 10.0f * std::log10 ((fastMs + 1.0e-12f) / (slowMs + 1.0e-7f));
            const float thr = 16.0f - 8.0f * p.smack;
            if (! smackArmed && crest < thr - 3.0f) smackArmed = true;
            if (smackArmed && crest > thr) { smackArmed = false; smackAge = 0; }
            const float window = smackAge < smackHold ? 1.0f
                                                      : std::max (0.0f, 1.0f - (float) (smackAge - smackHold) / (float) smackFade);
            if (smackAge < smackHold + smackFade) ++smackAge;
            smackTarget = std::clamp ((crest - thr) * 1.5f, 0.0f, 24.0f * p.smack) * window;
        }

        // --- gain delays (the gain leads the audio by the lookahead), then attack / release in dB
        const int gLen = delayLen + 1;
        smackDelay[(size_t) gainPos] = smackTarget;
        const float st = smackDelay[(size_t) ((gainPos - (delayLen - smackLook) + gLen) % gLen)];
        smackRed = st + (st > smackRed ? smackAtkC : smackRelC) * (smackRed - st);
        const float smackGain = dbToLin (-smackRed);

        splitMix += ((p.split ? 1.0f : 0.0f) - splitMix) * (1.0f - mixC);

        float maxRed = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            redDelay[c][(size_t) gainPos] = target[c];
            const float t = redDelay[c][(size_t) ((gainPos - (delayLen - look) + gLen) % gLen)];
            red[c] = t + (t > red[c] ? atkC : relC) * (red[c] - t);
            maxRed = std::max (maxRed, red[c]);
            const float gain = dbToLin (-red[c]);

            // --- delayed audio
            const float xd = audioDelay[c][(size_t) audioPos];
            audioDelay[c][(size_t) audioPos] = in[c];

            const float a = smackShelf[c].process (xd, smackGain, smackCoef);
            const float highsOnly = shelf[c].process (a, gain, xoCoef);
            const float wide = a * gain;
            const float y = wide + (highsOnly - wide) * splitMix;

            dry[c] = xd;
            out[c] = p.listen == 1 ? sc[c] : (p.listen == 2 ? xd - y : y);
        }
        audioPos = (audioPos + 1) % delayLen;
        gainPos = (gainPos + 1) % gLen;

        meterLevel = std::max (meterLevel, level);
        meterRed   = std::max (meterRed, maxRed);
        meterSmack = std::max (meterSmack, smackRed);
    }

    // --- meters: maxima since the last resetMeters()
    float getMeterLevelDb() const noexcept { return meterLevel; } // detector level in the threshold's domain
    float getMeterReductionDb() const noexcept { return meterRed; }
    float getMeterSmackDb() const noexcept { return meterSmack; }
    void resetMeters() noexcept { meterLevel = floorDb; meterRed = 0.0f; meterSmack = 0.0f; }

    /** Q of one of the two cascaded bandpass stages so that the pair is -3 dB at +-width/2 octaves. */
    static float stageQ (float widthOct) noexcept
    {
        const float h = std::exp2 (0.5f * widthOct);
        return 0.6423f / (h - 1.0f / h);
    }

    /** Detector filter magnitude at f, for display. */
    static float detectorMagnitude (float f, float freqHz, float widthOct, bool band) noexcept
    {
        const float r = f / freqHz;
        if (band)
        {
            const float q = stageQ (widthOct), d = r - 1.0f / r;
            return 1.0f / (1.0f + q * q * d * d); // two stages: |H|^2 of one stage
        }
        const float r2 = r * r;
        return r2 * r2 / (1.0f + r2 * r2);        // two Butterworth highpasses
    }

    /** Shelf corner used in split mode: below the detection band so its centre is fully treated. */
    static float splitFrequency (float freqHz, float widthOct, bool band) noexcept
    {
        return band ? 0.7f * freqHz * std::exp2 (-0.5f * widthOct) : 0.8f * freqHz;
    }

private:
    static constexpr int coefInterval = 16;

    float coef (float seconds) const noexcept { return std::exp (-1.0f / (seconds * (float) fs)); }
    static float dbToLin (float db) noexcept { return std::exp (db * 0.11512925f); }

    void updateCoefs() noexcept
    {
        logFreq += (std::log2 (std::clamp (p.freqHz, 200.0f, 20000.0f)) - logFreq) * paramC;
        width   += (std::clamp (p.widthOct, 0.2f, 4.0f) - width) * paramC;
        if (logFreq == lastLogFreq && width == lastWidth && p.bandDetect == lastBand) return;
        lastLogFreq = logFreq; lastWidth = width; lastBand = p.bandDetect;

        const float top = (float) fs * 0.45f;
        const float f = std::min (std::exp2 (logFreq), top);
        const float g = (float) std::tan (3.14159265358979 * f / fs);
        scCoef.set (g, p.bandDetect ? stageQ (width) : 0.70711f);
        const float fx = std::min (splitFrequency (f, width, p.bandDetect), top);
        xoCoef.set ((float) std::tan (3.14159265358979 * fx / fs), 0.70711f);
    }

    double fs = 44100.0;
    DeEsserParams p;
    int delayLen = 1, look = 0, smackLook = 0, audioPos = 0, gainPos = 0;
    int smackHold = 1, smackFade = 1, smackAge = 2;
    bool smackArmed = true;
    unsigned tick = 0;

    std::vector<float> audioDelay[maxChannels], redDelay[maxChannels], smackDelay;
    TptSvf scA[maxChannels], scB[maxChannels], smackHp[maxChannels];
    DynamicShelf smackShelf[maxChannels], shelf[maxChannels];
    TptCoef scCoef, xoCoef, smackCoef;

    float scMs[maxChannels] = {}, fullMs[maxChannels] = {}, red[maxChannels] = {};
    float fastMs = 0.0f, slowMs = 0.0f, smackRed = 0.0f;
    float envC = 0.0f, fastC = 0.0f, slowC = 0.0f, slowRelC = 0.0f, smackAtkC = 0.0f, smackRelC = 0.0f, mixC = 0.0f, paramC = 1.0f;
    float atkC = 0.0f, relC = 0.0f;
    float logFreq = 12.67f, width = 1.2f, lastLogFreq = -1.0f, lastWidth = -1.0f;
    bool lastBand = true;
    float splitMix = 1.0f;
    float meterLevel = floorDb, meterRed = 0.0f, meterSmack = 0.0f;
};
} // namespace sdsp
