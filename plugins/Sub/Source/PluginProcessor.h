#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <dsp/StackedLowpass.h>
#include "Params.h"

/** Lock-free meter values written by the audio thread, read (and reset) by the UI. */
struct SubMeters
{
    std::atomic<float> inPeak  { 0.0f };
    std::atomic<float> outPeak { 0.0f };

    static void storeMax (std::atomic<float>& a, float v) noexcept
    {
        auto cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
};

class SubProcessor : public juce::AudioProcessor
{
public:
    SubProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sub"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    SubMeters meters;

private:
    sdsp::StackedLowpass filter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffS;
    juce::SmoothedValue<float> resoS, layerS, driveS, driveWetS, mixS, outS;

    std::atomic<float>* pCutoff; std::atomic<float>* pReso; std::atomic<float>* pLayers;
    std::atomic<float>* pDrive;  std::atomic<float>* pMix;  std::atomic<float>* pOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubProcessor)
};
