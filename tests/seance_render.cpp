// A/B demo for Séance: a sparse guitar-like phrase, once dry and once through the real SeanceProcessor
// (bar 1 plain tape echo, bar 2 synced ping-pong, bar 3 dark dub feedback, bar 4 warped tape).
// Usage: seance_render <outDir>  ->  dry.wav, wet.wav
#include "../plugins/Seance/Source/PluginProcessor.h"
#include "ab_render.h"

namespace
{
juce::AudioBuffer<float> phrase (double seconds)
{
    const int n = (int) (seconds * ab::sr);
    juce::AudioBuffer<float> b (2, n); b.clear();
    static constexpr double notes[] = { 220.0, 261.63, 329.63, 293.66, 246.94, 196.0, 220.0, 164.81 };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / ab::sr;
        const double step = 1.0; // one note every half bar at 120 bpm
        const int k = (int) (t / step);
        const double tp = std::fmod (t, step);
        if (tp > 0.45) continue;
        const double f = notes[k % 8];
        const double env = std::exp (-tp * 8.0) * std::min (1.0, tp * 2000.0);
        const float s = 0.35f * (float) (env * (std::sin (ab::twoPi * f * tp) + 0.5 * std::sin (ab::twoPi * 2.0 * f * tp)
                                                + 0.2 * std::sin (ab::twoPi * 3.0 * f * tp)));
        b.setSample (0, i, s);
        b.setSample (1, i, s);
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
    auto dry = phrase (seconds);
    SeanceProcessor proc;
    const int total = dry.getNumSamples();
    int lastBar = -1;
    auto wet = ab::process (proc, dry, [&] (int pos)
    {
        const int bar = std::min (3, pos * 4 / total);
        if (bar == lastBar) return;
        lastBar = bar;
        static constexpr int presetForBar[] = { 0, 4, 5, 6 }; // INIT, PING PONG GHOSTS, DUB SPIRITS, WARPED TAPE
        proc.setCurrentProgram (presetForBar[bar]);
        ab::setParam (proc.apvts, "mix", 40.0f);
    });
    ab::fades (dry); ab::fades (wet);
    ab::writeWav (dry, outDir.getChildFile ("dry.wav"));
    ab::writeWav (wet, outDir.getChildFile ("wet.wav"));
    return 0;
}
