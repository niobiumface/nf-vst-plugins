// A/B demo for Sub: a bright saw chord progression with a light hi-hat, once dry and once through the
// real SubProcessor with 24 dB/oct and an automated cutoff sweep (2 kHz → 140 Hz → 2 kHz).
// Usage: sub_render <outDir>  ->  dry.wav, wet.wav
#include "../plugins/Sub/Source/PluginProcessor.h"
#include "ab_render.h"

namespace
{
juce::AudioBuffer<float> chords (double seconds)
{
    const int n = (int) (seconds * ab::sr);
    juce::AudioBuffer<float> b (2, n); b.clear();
    const int prog[4][4] = { { 45, 52, 57, 60 }, { 41, 48, 53, 57 }, { 43, 50, 55, 59 }, { 40, 47, 52, 55 } }; // Am F G Em
    const double bar = seconds / 4.0; ab::Rng rng;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / ab::sr; const int k = std::min (3, (int) (t / bar)); const double tb = t - k * bar;
        const float env = (float) std::min (1.0, tb * 30.0) * (float) std::min (1.0, (bar - tb) * 30.0);
        float l = 0.0f, r = 0.0f;
        for (int v = 0; v < 4; ++v)
        {
            const double f = 440.0 * std::pow (2.0, (prog[k][v] - 69) / 12.0);
            const float sa = (float) (2.0 * std::fmod (t * f * 1.003, 1.0) - 1.0), sb = (float) (2.0 * std::fmod (t * f * 0.997 + 0.3, 1.0) - 1.0);
            l += 0.07f * sa + 0.05f * sb; r += 0.05f * sa + 0.07f * sb;
        }
        const double beat = 60.0 / 100.0 / 2.0; const double tp = std::fmod (t, beat);           // eighth-note hats
        const float hat = 0.05f * rng.next() * (float) std::exp (-tp * 60.0);
        b.setSample (0, i, env * l + hat); b.setSample (1, i, env * r + hat);
    }
    return b;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    const double seconds = 9.6;
    auto dry = chords (seconds);
    SubProcessor proc;
    ab::setParam (proc.apvts, "layers", 2.0f);   // 24 dB/oct
    ab::setParam (proc.apvts, "reso", 28.0f);
    ab::setParam (proc.apvts, "drive", 4.0f);
    ab::setParam (proc.apvts, "mix", 100.0f);
    const int total = dry.getNumSamples();
    auto wet = ab::process (proc, dry, [&] (int pos)
    {
        const double x = pos / (double) total;                       // 0 … 1
        const double sweep = 0.5 - 0.5 * std::cos (x * 6.283185307); // 0 → 1 → 0
        ab::setParam (proc.apvts, "cutoff", (float) (2000.0 * std::pow (140.0 / 2000.0, sweep)));
    });
    ab::fades (dry); ab::fades (wet);
    ab::writeWav (dry, outDir.getChildFile ("dry.wav"));
    ab::writeWav (wet, outDir.getChildFile ("wet.wav"));
    return 0;
}
