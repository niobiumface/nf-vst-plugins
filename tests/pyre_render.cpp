// A/B demo for Pyre: a clean drum + bass + keys loop, once dry and once through the real PyreProcessor
// (tube -> tape -> fuzz -> fold, one bar each, drive rising inside every bar).
// Usage: pyre_render <outDir>  ->  dry.wav, wet.wav
#include "../plugins/Pyre/Source/PluginProcessor.h"
#include "ab_render.h"

namespace
{
juce::AudioBuffer<float> loop (double seconds)
{
    const int n = (int) (seconds * ab::sr);
    juce::AudioBuffer<float> b (2, n); b.clear();
    ab::Rng rng;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / ab::sr, beat = 0.5, tb = std::fmod (t, beat), te = std::fmod (t, beat * 0.5);
        const int beatIdx = (int) (std::fmod (t, beat * 4.0) / beat);
        const float kick  = 0.5f * (float) std::exp (-tb * 18.0) * (float) std::sin (ab::twoPi * (45.0 + 120.0 * std::exp (-tb * 40.0)) * tb);
        const float snare = (beatIdx == 1 || beatIdx == 3) ? 0.2f * (float) std::exp (-tb * 25.0) * rng.next() : 0.0f;
        const float bass  = 0.15f * (float) (2.0 * std::fmod (t * 55.0, 1.0) - 1.0) * (float) std::min (1.0, te * 200.0) * (float) std::exp (-te * 4.0);
        float keys = 0.0f;
        for (double f : { 220.0, 277.18, 329.63 })
            keys += 0.06f * (float) std::sin (ab::twoPi * f * t);
        b.setSample (0, i, kick + snare + bass + keys);
        b.setSample (1, i, kick + snare + bass + keys * 0.9f);
    }
    return b;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    const double seconds = 8.0; // 4 bars at 120 bpm
    auto dry = loop (seconds);
    PyreProcessor proc;
    ab::setParam (proc.apvts, "autogain", 1.0f);
    ab::setParam (proc.apvts, "mix", 100.0f);
    const int total = dry.getNumSamples();
    auto wet = ab::process (proc, dry, [&] (int pos)
    {
        const double x = pos / (double) total * 4.0;     // 0 … 4
        const int bar = std::min (3, (int) x);
        ab::setParam (proc.apvts, "mode", (float) bar);
        ab::setParam (proc.apvts, "drive", (float) (6.0 + 24.0 * (x - bar)));
    });
    ab::compensate (wet, proc.getLatencySamples());
    ab::fades (dry); ab::fades (wet);
    ab::writeWav (dry, outDir.getChildFile ("dry.wav"));
    ab::writeWav (wet, outDir.getChildFile ("wet.wav"));
    return 0;
}
