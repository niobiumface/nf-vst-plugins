// Offline sanity tests for shared/dsp (no JUCE). Build target: dsp_test
#include <dsp/NoiseGate.h>
#include <dsp/LookaheadLimiter.h>
#include <dsp/StackedLowpass.h>
#include <cstdio>
#include <cmath>
#include <random>

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
    }

    std::printf (failures == 0 ? "ALL DSP TESTS PASSED\n" : "%d DSP TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
