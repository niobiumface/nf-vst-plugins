// Offline sanity tests for shared/dsp (no JUCE). Build target: dsp_test
#include <dsp/NoiseGate.h>
#include <dsp/LookaheadLimiter.h>
#include <dsp/StackedLowpass.h>
#include <dsp/DeEsser.h>
#include <cstdio>
#include <cmath>
#include <random>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        // --- limiter: never exceeds ceiling, even with +24 dB hot noise and sine
        sdsp::LookaheadLimiter lim;
        lim.prepare (fs, 1.5f);
        lim.setParams (-1.0f, 50.0f);
        const float ceiling = sdsp::dbToGain (-1.0f);
        std::mt19937 rng (1);
        std::uniform_real_distribution<float> uni (-16.0f, 16.0f);
        float maxOut = 0.0f;
        for (int i = 0; i < (int) fs * 2; ++i)
        {
            float f[2];
            const float s = (i % 20000 < 10000) ? uni (rng) : 8.0f * (float) std::sin (i * 0.01);
            f[0] = s; f[1] = -0.5f * s;
            lim.processFrame (f, 2, true);
            maxOut = std::max ({ maxOut, std::abs (f[0]), std::abs (f[1]) });
        }
        CHECK (maxOut <= ceiling + 1e-6f, "limiter overshoot at %.0f Hz: %f > %f", fs, maxOut, ceiling);

        // --- limiter: quiet signal passes untouched (only delayed)
        lim.reset();
        const int L = lim.getLatency();
        float maxErr = 0.0f;
        std::vector<float> in ((size_t) fs / 10);
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * (float) std::sin ((double) i * 0.05);
        for (size_t i = 0; i < in.size(); ++i)
        {
            float f[1] = { in[i] };
            lim.processFrame (f, 1, true);
            if (i >= (size_t) L) maxErr = std::max (maxErr, std::abs (f[0] - in[i - (size_t) L]));
        }
        CHECK (maxErr < 1e-6f, "limiter altered quiet signal at %.0f Hz: err %f", fs, maxErr);

        // --- gate: closes on silence-level noise, opens on loud signal
        sdsp::NoiseGate gate;
        gate.prepare (fs);
        gate.setParams (-40.0f, -80.0f, 0.5f, 20.0f, 50.0f);
        float g = 1.0f;
        for (int i = 0; i < (int) fs; ++i) g = gate.process (0.001f); // -60 dB
        CHECK (g < 0.001f && ! gate.isOpen(), "gate did not close at %.0f Hz (g=%f)", fs, g);
        for (int i = 0; i < (int) (fs * 0.01); ++i) g = gate.process (0.5f);
        CHECK (g > 0.99f && gate.isOpen(), "gate did not open at %.0f Hz (g=%f)", fs, g);

        // --- stacked lowpass: passes lows, kills highs harder with more layers, stable at max resonance
        auto sineGain = [fs] (float freq, float cutoff, float reso, float layers)
        {
            sdsp::StackedLowpass f;
            f.prepare (fs);
            float pk = 0.0f;
            const int n = (int) fs;
            for (int i = 0; i < n; ++i)
            {
                f.setParams (cutoff, reso);
                const float y = f.process (0, (float) std::sin (2.0 * 3.14159265358979 * freq * i / fs), layers);
                if (i > n / 2) pk = std::max (pk, std::abs (y));
            }
            return pk;
        };
        const float pass = sineGain (50.0f, 1000.0f, 0.0f, 4.0f);
        CHECK (pass > 0.95f && pass < 1.05f, "lowpass passband gain %f at %.0f Hz", pass, fs);
        const float s1 = sineGain (8000.0f, 1000.0f, 0.0f, 1.0f), s4 = sineGain (8000.0f, 1000.0f, 0.0f, 4.0f);
        CHECK (s1 < 0.03f && s4 < s1 * 0.001f, "lowpass stopband 1L=%f 4L=%f at %.0f Hz", s1, s4, fs);
        const float res = sineGain (1000.0f, 1000.0f, 1.0f, 4.0f);
        CHECK (std::isfinite (res) && res < 20.0f, "resonance unstable (%f) at %.0f Hz", res, fs);
        const float top = sineGain (100.0f, 20000.0f, 1.0f, 4.0f);
        CHECK (std::isfinite (top), "cutoff clamp failed at %.0f Hz", fs);

        // --- de-esser
        {
            const double twoPi = 6.283185307179586;
            // runs a mono signal through the de-esser, returns the output peak over the last quarter
            auto run = [fs] (const sdsp::DeEsserParams& dp, auto&& signal, double seconds, float* peakAll = nullptr)
            {
                sdsp::DeEsser de;
                de.prepare (fs);
                de.setParams (dp);
                de.reset();
                const int n = (int) (fs * seconds);
                float pk = 0.0f, all = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    float in[1] = { signal (i) }, out[1], dry[1];
                    de.processFrame (in, out, dry, 1);
                    all = std::max (all, std::abs (out[0]));
                    if (i > n * 3 / 4) pk = std::max (pk, std::abs (out[0]));
                }
                if (peakAll != nullptr) *peakAll = all;
                return pk;
            };
            auto sine = [fs, twoPi] (double f, float a) { return [=] (int i) { return a * (float) std::sin (twoPi * f * i / fs); }; };

            sdsp::DeEsserParams dp; // relative, band, split, -14 dB, range 9 dB

            // a pure sibilance-band tone is reduced by about the range, in split and wideband mode
            for (bool split : { true, false })
            {
                dp.split = split;
                const float ess = run (dp, sine (6500.0, 0.25f), 0.5);
                const float redDb = 20.0f * std::log10 (ess / 0.25f);
                CHECK (redDb < -7.0f && redDb > -9.5f, "de-esser reduction %.2f dB (split=%d) at %.0f Hz", redDb, (int) split, fs);
                // a low tone is left alone
                const float low = run (dp, sine (200.0, 0.25f), 0.5);
                CHECK (std::abs (20.0f * std::log10 (low / 0.25f)) < 0.2f, "de-esser touched a 200 Hz tone (split=%d): %f at %.0f Hz", (int) split, low, fs);
            }
            dp.split = true;

            // vowel + sibilant: only the highs go down in split mode, and never by more than the range
            {
                const float mixPk = run (dp, [&] (int i) { return sine (200.0, 0.05f) (i) + sine (7000.0, 0.25f) (i); }, 0.5);
                CHECK (mixPk < 0.25f && mixPk > 0.05f + 0.25f * 0.33f, "de-esser mixed signal peak %f at %.0f Hz", mixPk, fs);
            }

            // no reduction = a pure delay, wideband and split (the dynamic shelf is transparent at unity)
            for (bool split : { false, true })
            {
                sdsp::DeEsser de;
                de.prepare (fs);
                auto t = dp; t.rangeDb = 0.0f; t.split = split; t.smack = 1.0f;
                de.setParams (t); de.reset();
                const int L = de.getLatency();
                std::vector<float> in ((size_t) (fs * 0.5));
                float maxErr = 0.0f;
                for (size_t i = 0; i < in.size(); ++i)
                {
                    in[i] = 0.4f * (float) std::sin ((double) i * 0.9) + 0.2f * (float) std::sin ((double) i * 0.011);
                    float f[1] = { in[i] }, d[1];
                    de.processFrame (f, f, d, 1);
                    if (i >= (size_t) L + (size_t) (fs * 0.3)) maxErr =std::max (maxErr, std::abs (f[0] - in[i - (size_t) L]));
                }
                CHECK (maxErr < 2e-5f, "de-esser not transparent without reduction (split=%d): err %g at %.0f Hz", (int) split, maxErr, fs);
                CHECK (L == (int) std::lround (0.005 * fs), "de-esser latency %d at %.0f Hz", L, fs);
            }

            // smack: a 1.5 ms click out of silence is ducked, a sustained tone recovers
            {
                auto t = dp; t.rangeDb = 0.0f;
                const int start = (int) (fs * 0.3), len = (int) (fs * 0.0015);
                auto click = [&] (int i) { return (i >= start && i < start + len) ? sine (5000.0, 0.5f) (i - start) : 0.0f; };
                float off = 0.0f, on = 0.0f;
                run (t, click, 0.4, &off);
                t.smack = 1.0f;
                run (t, click, 0.4, &on);
                CHECK (on < off * 0.3f, "smack did not duck a click: %f -> %f at %.0f Hz", off, on, fs);
                // ... also 100 ms after a loud sound has ended
                auto wordThenClick = [&] (int i) { return i < start - (int) (fs * 0.1) ? sine (5000.0, 0.25f) (i) : click (i); };
                auto peakAfter = [&] (const sdsp::DeEsserParams& q)
                {
                    sdsp::DeEsser de; de.prepare (fs); de.setParams (q); de.reset();
                    float pk = 0.0f;
                    for (int i = 0; i < (int) (fs * 0.4); ++i)
                    {
                        float f[1] = { wordThenClick (i) }, d[1];
                        de.processFrame (f, f, d, 1);
                        if (i > start - (int) (fs * 0.05)) pk = std::max (pk, std::abs (f[0]));
                    }
                    return pk;
                };
                auto noSmack = t; noSmack.smack = 0.0f;
                const float late = peakAfter (t), lateOff = peakAfter (noSmack);
                CHECK (late < lateOff * 0.5f, "smack missed a click after a word: %f -> %f at %.0f Hz", lateOff, late, fs);
                const float sus = run (t, sine (5000.0, 0.25f), 1.0);
                CHECK (std::abs (20.0f * std::log10 (sus / 0.25f)) < 0.5f, "smack ducks a sustained tone: %f at %.0f Hz", sus, fs);
            }

            // stays finite with hot noise in every mode
            {
                std::mt19937 nrng (7);
                std::uniform_real_distribution<float> nuni (-4.0f, 4.0f);
                for (int mode = 0; mode < 8; ++mode)
                {
                    auto t = dp; t.relative = mode & 1; t.bandDetect = mode & 2; t.split = mode & 4; t.smack = 0.7f; t.rangeDb = 24.0f; t.freqHz = 16000.0f;
                    float all = 0.0f;
                    run (t, [&] (int) { return nuni (nrng); }, 0.2, &all);
                    CHECK (std::isfinite (all) && all < 16.0f, "de-esser unstable in mode %d: %f at %.0f Hz", mode, all, fs);
                }
            }
        }
    }

    std::printf (failures == 0 ? "ALL DSP TESTS PASSED\n" : "%d DSP TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
