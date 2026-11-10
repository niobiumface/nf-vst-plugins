// A/B demo for CUtTA: a drum loop recorded "in a room" (noise floor + hi-hat bleed between the hits),
// once dry and once through the real CuttaProcessor (gate + lookahead limiter).
// Usage: cutta_render <outDir>  ->  dry.wav, wet.wav
#include "../plugins/CUtTA/Source/PluginProcessor.h"
#include "ab_render.h"

namespace
{
juce::AudioBuffer<float> drumLoop (double seconds)
{
    const int n = (int) (seconds * ab::sr);
    juce::AudioBuffer<float> b (2, n); b.clear();
    ab::Rng rng; const double bpm = 96.0, beat = 60.0 / bpm;
    auto add = [&] (int c, int i, float v) { if (i >= 0 && i < n) b.addSample (c, i, v); };
    for (double t = 0.0; t < seconds; t += beat / 2.0)
    {
        const int s0 = (int) (t * ab::sr), step = (int) std::lround (t / (beat / 2.0)) % 8;
        if (step == 0 || step == 3 || step == 5)                              // kick
            for (int i = 0; i < (int) (0.35 * ab::sr); ++i) { const float x = i / (float) ab::sr, f = 48.0f + 110.0f * std::exp (-x * 28.0f);
                const float v = 0.85f * std::sin (ab::twoPi * f * x) * std::exp (-x * 7.0f); add (0, s0 + i, v); add (1, s0 + i, v); }
        if (step == 2 || step == 6)                                           // snare
            for (int i = 0; i < (int) (0.25 * ab::sr); ++i) { const float x = i / (float) ab::sr, e = std::exp (-x * 16.0f);
                const float v = 0.42f * (0.6f * rng.next() + 0.4f * std::sin (ab::twoPi * 190.0f * x)) * e; add (0, s0 + i, v); add (1, s0 + i, v * 0.95f); }
        for (int i = 0; i < (int) (0.05 * ab::sr); ++i)                       // hi-hat bleed, quiet
        { const float v = 0.035f * rng.next() * std::exp (-(i / (float) ab::sr) * 70.0f); add (0, s0 + i, v * 0.8f); add (1, s0 + i, v); }
    }
    for (int i = 0; i < n; ++i)                                               // room noise + 50 Hz hum
    { const float hum = 0.006f * std::sin (ab::twoPi * 50.0f * i / (float) ab::sr); add (0, i, 0.012f * rng.next() + hum); add (1, i, 0.012f * rng.next() + hum); }
    return b;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    auto dry = drumLoop (60.0 / 96.0 * 16.0); // 4 bars
    CuttaProcessor proc;
    ab::setParam (proc.apvts, "gateThresh", -30.0f);
    ab::setParam (proc.apvts, "gateRange", -80.0f);
    ab::setParam (proc.apvts, "gateAttack", 0.5f);
    ab::setParam (proc.apvts, "gateHold", 60.0f);
    ab::setParam (proc.apvts, "gateRelease", 90.0f);
    ab::setParam (proc.apvts, "limDrive", 6.0f);
    ab::setParam (proc.apvts, "limCeiling", -0.3f);
    auto wet = ab::process (proc, dry);
    ab::compensate (wet, proc.getLatencySamples());
    ab::fades (dry); ab::fades (wet);
    ab::writeWav (dry, outDir.getChildFile ("dry.wav"));
    ab::writeWav (wet, outDir.getChildFile ("wet.wav"));
    std::printf ("latency %d samples\n", proc.getLatencySamples());
    return 0;
}
