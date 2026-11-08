#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <dsp/NoiseGate.h>
#include <dsp/LookaheadLimiter.h>
#include "Params.h"
#include "Presets.h"

/** Lock-free meter values written by the audio thread, read (and reset) by the UI. */
struct CuttaMeters
{
    std::atomic<float> inPeak   { 0.0f };  // pre-gate input, linear, max since last read
    std::atomic<float> preLim   { 0.0f };  // after gate + drive, linear
    std::atomic<float> outPeak  { 0.0f };  // output, linear
    std::atomic<float> gateGain { 0.0f };  // current gate gain, linear
    std::atomic<float> limGain  { 1.0f };  // min limiter gain since last read
    std::atomic<bool>  gateOpen { false };

    static void storeMax (std::atomic<float>& a, float v) noexcept
    {
        auto cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
    static void storeMin (std::atomic<float>& a, float v) noexcept
    {
        auto cur = a.load (std::memory_order_relaxed);
        while (v < cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
};

class CuttaProcessor : public juce::AudioProcessor
{
public:
    CuttaProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CUtTA"; }
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
    CuttaMeters meters;

private:
    sdsp::NoiseGate gate;
    sdsp::LookaheadLimiter limiter;
    juce::SmoothedValue<float> driveGain;
    int currentPreset = 0;

    std::atomic<float>* pGateOn; std::atomic<float>* pThresh; std::atomic<float>* pRange;
    std::atomic<float>* pAttack; std::atomic<float>* pHold;   std::atomic<float>* pRelease;
    std::atomic<float>* pLimOn;  std::atomic<float>* pDrive;  std::atomic<float>* pCeiling;
    std::atomic<float>* pLimRel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CuttaProcessor)
};
