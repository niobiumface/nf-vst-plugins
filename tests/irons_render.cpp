// A/B demo for Iron S: a synthetic "voice" (sung vowels with harsh "s"/"sh" sounds and a few lip smacks),
// once dry and once through the real IronSProcessor (INIT preset + SMACK 70 %), latency-compensated.
// Usage: irons_render <outDir>  ->  dry.wav, wet.wav
#include "../plugins/IronS/Source/PluginProcessor.h"
#include "ab_render.h"

namespace
{
/** 2.4 s phrase, looped: vowel – "s" – vowel – "sh" – vowel, lip smacks in the gaps. Formant-ish harmonics, bright noise for sibilants. */
juce::AudioBuffer<float> voice (double seconds)
{
    const int n = (int) (seconds * ab::sr);
    juce::AudioBuffer<float> b (2, n); b.clear();
    ab::Rng rng; float h1 = 0.0f, h2 = 0.0f, last = 0.0f, bp = 0.0f, bp2 = 0.0f;
    const double notes[] = { 196.0, 220.0, 246.9, 220.0 };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / ab::sr, p = std::fmod (t, 2.4);
        const int k = (int) (t / 2.4) % 4;
        float y = 0.0f;
        auto vowel = [&] (double a, double e, double f0)
        {
            if (p < a || p >= e) return 0.0f;
            const float env = (float) std::min (1.0, std::min (p - a, e - p) * 25.0);
            float s = 0.0f;
            for (int h = 1; h <= 16; ++h)
            {
                const double f = f0 * h; // formants around 700 Hz and 1.2 kHz
                const float g = (float) (1.0 / h) * (float) (1.0 + 2.5 * std::exp (-std::pow ((f - 700.0) / 250.0, 2)) + 1.5 * std::exp (-std::pow ((f - 1200.0) / 300.0, 2)));
                s += g * (float) std::sin (6.283185307 * f * t + h);
            }
            return env * 0.11f * s;
        };
        y += vowel (0.00, 0.55, notes[k]) + vowel (0.80, 1.35, notes[k] * 1.122) + vowel (1.65, 2.25, notes[k] * 0.891);
        // sibilants: highpassed noise with a resonance around 7 kHz ("s") or 4 kHz ("sh")
        const float wn = rng.next();
        h1 += 0.55f * (wn - last - h1); last = wn; h2 += 0.5f * (h1 - h2);
        const double fc = (p >= 1.35 && p < 1.65) ? 4200.0 : 7200.0, w = 6.283185307 * fc / ab::sr;
        bp += (float) w * (wn - bp - 0.35f * bp2); bp2 += (float) w * bp;
        auto sib = [&] (double a, double e, float gain) { return (p >= a && p < e) ? gain * (float) std::min (1.0, std::min (p - a, e - p) * 40.0) : 0.0f; };
        y += sib (0.55, 0.80, 0.42f) * (h1 - 0.5f * h2) + sib (1.35, 1.65, 0.5f) * bp;
        // lip smacks: 2 ms clicks in the gaps
        if ((p >= 0.78 && p < 0.782) || (p >= 2.30 && p < 2.302)) y += 0.5f * wn;
        b.setSample (0, i, y); b.setSample (1, i, y);
    }
    return b;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    auto dry = voice (9.6);
    IronSProcessor proc;
    proc.setCurrentProgram (0);         // INIT
    ab::setParam (proc.apvts, "smack", 70.0f);
    auto wet = ab::process (proc, dry);
    ab::compensate (wet, proc.getLatencySamples());
    ab::fades (dry); ab::fades (wet);
    ab::writeWav (dry, outDir.getChildFile ("dry.wav"));
    ab::writeWav (wet, outDir.getChildFile ("wet.wav"));
    std::printf ("latency %d samples\n", proc.getLatencySamples());
    return 0;
}
