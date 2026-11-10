// Shared helpers for the A/B demo renderers (cutta_render.cpp, sub_render.cpp):
// deterministic test signals, parameter setting via the real APVTS, block-wise processing and WAV export.
// The wet file is produced by the plugin's own AudioProcessor – exactly the code inside the VST3.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <functional>

namespace ab
{
constexpr double sr = 44100.0;
constexpr int block = 256;
constexpr float twoPi = 6.283185307f;

struct Rng { uint32_t s = 0x9e3779b9u; float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s / 4294967296.0f) * 2.0f - 1.0f; } };

inline void setParam (juce::AudioProcessorValueTreeState& v, const char* id, float value)
{
    if (auto* p = v.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value));
}

/** Processes `in` through `proc` block by block; `onBlock(sampleIndex)` may change parameters (automation). */
inline juce::AudioBuffer<float> process (juce::AudioProcessor& proc, const juce::AudioBuffer<float>& in, std::function<void (int)> onBlock = {})
{
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        const int n = std::min (block, out.getNumSamples() - pos);
        if (onBlock) onBlock (pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
        proc.processBlock (view, midi);
    }
    proc.releaseResources();
    return out;
}

/** Removes the plugin's reported latency so dry and wet line up for A/B switching. */
inline void compensate (juce::AudioBuffer<float>& b, int latency)
{
    if (latency <= 0) return;
    const int n = b.getNumSamples();
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        auto* d = b.getWritePointer (c);
        std::memmove (d, d + latency, sizeof (float) * (size_t) (n - latency));
        std::fill (d + n - latency, d + n, 0.0f);
    }
}

inline void writeWav (const juce::AudioBuffer<float>& b, const juce::File& f)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    std::printf ("%s\n", f.getFullPathName().toRawUTF8());
}

/** Short fade-in/out so the loop doesn't click when the player repeats it. */
inline void fades (juce::AudioBuffer<float>& b, int samples = 441)
{
    const int n = b.getNumSamples();
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        b.applyGainRamp (c, 0, samples, 0.0f, 1.0f);
        b.applyGainRamp (c, n - samples, samples, 1.0f, 0.0f);
    }
}
} // namespace ab
