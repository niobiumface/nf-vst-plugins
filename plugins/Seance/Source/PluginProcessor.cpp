#include "PluginProcessor.h"
#include "PluginEditor.h"

SeanceProcessor::SeanceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Seance", params::createLayout())
{
    pTime   = apvts.getRawParameterValue (params::time);
    pSync   = apvts.getRawParameterValue (params::sync);
    pDiv    = apvts.getRawParameterValue (params::division);
    pFb     = apvts.getRawParameterValue (params::feedback);
    pWow    = apvts.getRawParameterValue (params::wow);
    pLow    = apvts.getRawParameterValue (params::lowCut);
    pHigh   = apvts.getRawParameterValue (params::highCut);
    pPing   = apvts.getRawParameterValue (params::pingPong);
    pMix    = apvts.getRawParameterValue (params::mix);
    pBypass = apvts.getRawParameterValue (params::bypass);
    bypassParam = apvts.getParameter (params::bypass);
}

bool SeanceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

namespace
{
/** Delays usually keep the dry signal at full level up to 50 % mix and fade the repeats in (and vice versa). */
float dryGain (float mix) noexcept { return juce::jmin (1.0f, 2.0f * (1.0f - mix)); }
float wetGain (float mix) noexcept { return juce::jmin (1.0f, 2.0f * mix); }
} // namespace

float SeanceProcessor::targetDelayMs() const noexcept
{
    const float ms = pSync->load() > 0.5f ? params::divisionMs (juce::roundToInt (pDiv->load()), hostBpm.load())
                                          : pTime->load();
    return juce::jlimit (params::timeMin, params::maxDelaySeconds * 1000.0f, ms);
}

void SeanceProcessor::updateDelayParams() noexcept
{
    sdsp::TapeDelay::Params p;
    p.delaySamples = targetDelayMs() * 0.001f * (float) sr;
    p.feedback  = pFb->load() * 0.01f;
    p.wow       = pWow->load() * 0.01f;
    p.lowCutHz  = pLow->load();
    p.highCutHz = pHigh->load();
    p.pingPong  = pPing->load() > 0.5f ? 1.0f : 0.0f;
    delay.setParams (p);
}

void SeanceProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    delay.prepare (sampleRate, params::maxDelaySeconds);
    updateDelayParams();
    delay.snap();

    inOnset.prepare (sampleRate);
    inOnset.floor = 0.01f;
    for (auto& o : echoOnset) o.prepare (sampleRate);

    const float mix = pMix->load() * 0.01f;
    auto init = [sampleRate] (auto& s, float v) { s.reset (sampleRate, 0.03); s.setCurrentAndTargetValue (v); };
    init (dryS, dryGain (mix));
    init (wetS, wetGain (mix));
    init (bypassS, pBypass->load() > 0.5f ? 1.0f : 0.0f);
}

void SeanceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    for (int c = numIn; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
    if (numCh == 0 || numSamples == 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0) hostBpm.store (*bpm);

    updateDelayParams();
    const float mix = pMix->load() * 0.01f;
    dryS.setTargetValue (dryGain (mix));
    wetS.setTargetValue (wetGain (mix));
    bypassS.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (numCh > 1 ? 1 : 0);
    const bool monoIn = numIn < 2; // mono in -> stereo out: the input feeds both sides
    const bool stereoOut = numCh > 1;

    float inPk = 0.0f, wetPk = 0.0f, echoPk[2] = { 0.0f, 0.0f };
    uint32_t inCount = 0, echoCount[2] = { 0, 0 };

    for (int i = 0; i < numSamples; ++i)
    {
        const float xl = L[i];
        const float xr = (monoIn || ! stereoOut) ? xl : R[i];
        const float byp = bypassS.getNextValue();
        const float dry = dryS.getNextValue(), wetG = wetS.getNextValue() * (1.0f - byp);
        const float inG = 1.0f - byp; // bypassed: nothing new goes onto the tape, the old echoes fade out unheard

        float wl, wr;
        delay.process (xl * inG, xr * inG, wl, wr);

        inPk = std::max ({ inPk, std::abs (xl), std::abs (xr) });
        wetPk = std::max ({ wetPk, std::abs (wl), std::abs (wr) });
        if (inOnset.process (0.5f * (xl + xr) * inG) > 0.0f) ++inCount;
        for (int c = 0; c < 2; ++c)
            if (const float e = echoOnset[c].process (c == 0 ? wl : wr); e > 0.0f)
            {
                ++echoCount[c];
                echoPk[c] = std::max (echoPk[c], e);
            }

        const float dg = dry + (1.0f - dry) * byp;
        if (stereoOut)
        {
            L[i] = xl * dg + wl * wetG;
            R[i] = xr * dg + wr * wetG;
        }
        else
        {
            L[i] = xl * dg + 0.5f * (wl + wr) * wetG;
        }
    }

    // safety net: a blown-up state must never stay stuck
    if (! std::isfinite (wetPk))
    {
        delay.reset();
        buffer.clear();
        wetPk = 0.0f;
    }

    SeanceMeters::storeMax (meters.inPeak, inPk);
    SeanceMeters::storeMax (meters.wetPeak, wetPk);
    meters.inOnsets.fetch_add (inCount, std::memory_order_relaxed);
    for (int c = 0; c < 2; ++c)
    {
        meters.echoes[c].fetch_add (echoCount[c], std::memory_order_relaxed);
        SeanceMeters::storeMax (meters.echoLevel[c], echoPk[c]);
    }
    meters.delayMs.store (delay.getCurrentDelaySamples() * 1000.0f / (float) sr, std::memory_order_relaxed);
    meters.bpm.store ((float) hostBpm.load(), std::memory_order_relaxed);
}

void SeanceProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, presets::count)) return;
    currentPreset = index;
    apvts.state.setProperty ("preset", index, nullptr);

    for (auto& [id, value] : presets::values (presets::factory[index]))
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
}

const juce::String SeanceProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, presets::count) ? juce::String (presets::factory[index].name) : juce::String();
}

bool SeanceProcessor::isPresetModified() const
{
    const auto& pr = presets::factory[currentPreset];
    for (auto& [id, value] : presets::values (pr))
    {
        if (pr.sync && juce::String (id) == params::time) continue;        // not in use while synced
        if (! pr.sync && juce::String (id) == params::division) continue;
        if (auto* p = apvts.getParameter (id))
        {
            const float v = p->convertFrom0to1 (p->getValue());
            if (std::abs (v - value) > std::max (0.051f, std::abs (value) * 0.002f))
                return true;
        }
    }
    return false;
}

void SeanceProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SeanceProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, presets::count - 1, (int) apvts.state.getProperty ("preset", 0));
        }
}

juce::AudioProcessorEditor* SeanceProcessor::createEditor() { return new SeanceEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SeanceProcessor(); }
