#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <dsp/DeEsser.h>
#include "Params.h"
#include "Presets.h"

/** Lock-free values written by the audio thread, read (and reset) by the UI. */
struct IronMeters
{
    std::atomic<float> inPeak   { 0.0f };    // linear, max since last read
    std::atomic<float> outPeak  { 0.0f };
    std::atomic<float> level    { -60.0f };  // detector level in dB (threshold domain), max since last read
    std::atomic<float> reduction { 0.0f };   // de-ess gain reduction in dB (positive), max since last read
    std::atomic<float> smack    { 0.0f };    // smack gain reduction in dB (positive), max since last read

    static void storeMax (std::atomic<float>& a, float v) noexcept
    {
        auto cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
};

/** Ring of the most recent mono input (latency-aligned) and output samples for the UI spectrum. */
struct IronScope
{
    static constexpr int size = 4096; // power of two

    void push (float inSample, float outSample) noexcept
    {
        const int w = pos.load (std::memory_order_relaxed);
        in[(size_t) w].store (inSample, std::memory_order_relaxed);
        out[(size_t) w].store (outSample, std::memory_order_relaxed);
        pos.store ((w + 1) & (size - 1), std::memory_order_release);
    }

    /** Copies the newest n samples (oldest first). UI thread. */
    void read (float* inDest, float* outDest, int n) const noexcept
    {
        const int w = pos.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
        {
            const auto idx = (size_t) ((w - n + i + size) & (size - 1));
            inDest[i]  = in[idx].load (std::memory_order_relaxed);
            outDest[i] = out[idx].load (std::memory_order_relaxed);
        }
    }

    std::array<std::atomic<float>, size> in {}, out {};
    std::atomic<int> pos { 0 };
};

class IronSProcessor : public juce::AudioProcessor
{
public:
    IronSProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Iron S"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return presets::count; }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    /** True if any parameter differs from the current factory preset. */
    bool isPresetModified() const;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    IronMeters meters;
    IronScope scope;

private:
    sdsp::DeEsser deEsser;
    juce::SmoothedValue<float> outGain, bypassMix;
    int currentPreset = 0;

    std::atomic<float>* pThresh; std::atomic<float>* pRange; std::atomic<float>* pFreq;  std::atomic<float>* pWidth;
    std::atomic<float>* pSpeed;  std::atomic<float>* pSmack; std::atomic<float>* pLook;  std::atomic<float>* pLink;
    std::atomic<float>* pOut;    std::atomic<float>* pMode;  std::atomic<float>* pDetect; std::atomic<float>* pCut;
    std::atomic<float>* pListen; std::atomic<float>* pBypass;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IronSProcessor)
};
