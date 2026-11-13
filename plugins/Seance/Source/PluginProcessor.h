#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <dsp/TapeDelay.h>
#include "Params.h"
#include "Presets.h"

/** Lock-free values written by the audio thread, read (and reset) by the UI. */
struct SeanceMeters
{
    std::atomic<float> inPeak  { 0.0f };       // linear, max since last read
    std::atomic<float> wetPeak { 0.0f };       // the repeats (before the mix)
    std::atomic<uint32_t> inOnsets { 0 };      // counts onsets of the dry input
    std::atomic<uint32_t> echoes[2] { { 0 }, { 0 } }; // counts echo arrivals per side (onsets on the wet signal)
    std::atomic<float> echoLevel[2] { { 0.0f }, { 0.0f } }; // loudest echo onset per side since last read
    std::atomic<float> delayMs { 375.0f };     // current (gliding) delay time
    std::atomic<float> bpm { 120.0f };

    static void storeMax (std::atomic<float>& a, float v) noexcept
    {
        auto cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
};

class SeanceProcessor : public juce::AudioProcessor
{
public:
    SeanceProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Seance"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return presets::count; }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    /** True if any parameter differs from the current factory preset. */
    bool isPresetModified() const;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** The delay time in ms the parameters ask for right now (sync resolved with the host tempo). */
    float targetDelayMs() const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    SeanceMeters meters;

private:
    sdsp::TapeDelay delay;
    sdsp::OnsetDetector inOnset, echoOnset[2];
    juce::SmoothedValue<float> dryS, wetS, bypassS;
    double sr = 48000.0;
    std::atomic<double> hostBpm { 120.0 };
    int currentPreset = 0;

    std::atomic<float>* pTime; std::atomic<float>* pSync; std::atomic<float>* pDiv; std::atomic<float>* pFb;
    std::atomic<float>* pWow;  std::atomic<float>* pLow;  std::atomic<float>* pHigh; std::atomic<float>* pPing;
    std::atomic<float>* pMix;  std::atomic<float>* pBypass;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    void updateDelayParams() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeanceProcessor)
};
