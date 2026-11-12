#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <dsp/Saturator.h>
#include "Params.h"
#include "Presets.h"

/** Lock-free values written by the audio thread, read (and reset) by the UI. */
struct PyreMeters
{
    std::atomic<float> inPeak  { 0.0f };  // linear, max since last read
    std::atomic<float> outPeak { 0.0f };
    std::atomic<float> heat    { 0.0f };  // peak of the driven signal entering the shaper (1 = the curve's knee)

    static void storeMax (std::atomic<float>& a, float v) noexcept
    {
        auto cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
};

class PyreProcessor : public juce::AudioProcessor
{
public:
    PyreProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pyre"; }
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
    PyreMeters meters;

    static constexpr int osFactorLog2 = 2; // 4x oversampling

private:
    juce::dsp::Oversampling<float> oversampling { 2, osFactorLog2,
                                                  juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    sdsp::Saturator sat;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveS, compS;
    juce::SmoothedValue<float> biasS, lowS, highS, mixS, outS, bypassS;
    int maxBlock = 512;
    int currentPreset = 0;

    std::atomic<float>* pDrive; std::atomic<float>* pMode; std::atomic<float>* pBias; std::atomic<float>* pTone;
    std::atomic<float>* pMix;   std::atomic<float>* pOut;  std::atomic<float>* pAuto; std::atomic<float>* pBypass;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PyreProcessor)
};
