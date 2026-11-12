#include "PluginProcessor.h"
#include "PluginEditor.h"

PyreProcessor::PyreProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Pyre", params::createLayout())
{
    pDrive  = apvts.getRawParameterValue (params::drive);
    pMode   = apvts.getRawParameterValue (params::mode);
    pBias   = apvts.getRawParameterValue (params::bias);
    pTone   = apvts.getRawParameterValue (params::tone);
    pMix    = apvts.getRawParameterValue (params::mix);
    pOut    = apvts.getRawParameterValue (params::output);
    pAuto   = apvts.getRawParameterValue (params::autoGain);
    pBypass = apvts.getRawParameterValue (params::bypass);
    bypassParam = apvts.getParameter (params::bypass);
}

bool PyreProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

namespace
{
struct Targets { float drive, bias, low, high, comp, mix, out, bypass; int mode; };

Targets readTargets (std::atomic<float>* d, std::atomic<float>* m, std::atomic<float>* b, std::atomic<float>* t,
                     std::atomic<float>* mx, std::atomic<float>* o, std::atomic<float>* a, std::atomic<float>* byp) noexcept
{
    Targets r;
    r.mode   = juce::jlimit (0, sdsp::Saturator::numModes - 1, juce::roundToInt (m->load()));
    r.drive  = juce::Decibels::decibelsToGain (d->load());
    r.bias   = b->load() * 0.01f * sdsp::Saturator::maxBias;
    const float tone = t->load() * 0.01f;
    r.low    = sdsp::Saturator::tiltLowGain (tone);
    r.high   = sdsp::Saturator::tiltHighGain (tone);
    r.comp   = a->load() > 0.5f ? sdsp::Saturator::autoGain (r.mode, r.drive, r.bias) : 1.0f;
    r.mix    = mx->load() * 0.01f;
    r.out    = juce::Decibels::decibelsToGain (o->load());
    r.bypass = byp->load() > 0.5f ? 1.0f : 0.0f;
    return r;
}
} // namespace

void PyreProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (1, samplesPerBlock);
    oversampling.initProcessing ((size_t) maxBlock);
    oversampling.reset();
    setLatencySamples (juce::roundToInt (oversampling.getLatencyInSamples())); // integer by construction, constant

    const double osRate = sampleRate * (double) (1 << osFactorLog2);
    sat.prepare (osRate);

    const auto t = readTargets (pDrive, pMode, pBias, pTone, pMix, pOut, pAuto, pBypass);
    sat.setMode (t.mode);
    sat.reset();
    auto init = [osRate] (auto& s, float v, double seconds) { s.reset (osRate, seconds); s.setCurrentAndTargetValue (v); };
    init (driveS, t.drive, 0.03);
    init (compS, t.comp, 0.05);
    init (biasS, t.bias, 0.03);
    init (lowS, t.low, 0.03);
    init (highS, t.high, 0.03);
    init (mixS, t.mix, 0.03);
    init (outS, t.out, 0.03);
    init (bypassS, t.bypass, 0.02);
}

void PyreProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
    if (numCh == 0 || numSamples == 0) return;

    const auto t = readTargets (pDrive, pMode, pBias, pTone, pMix, pOut, pAuto, pBypass);
    sat.setMode (t.mode);
    driveS.setTargetValue (t.drive);
    compS.setTargetValue (t.comp);
    biasS.setTargetValue (t.bias);
    lowS.setTargetValue (t.low);
    highS.setTargetValue (t.high);
    mixS.setTargetValue (t.mix);
    outS.setTargetValue (t.out);
    bypassS.setTargetValue (t.bypass);

    float inPk = 0.0f, outPk = 0.0f, heatPk = 0.0f;
    juce::dsp::AudioBlock<float> whole (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) numSamples);

    for (int pos = 0; pos < numSamples; pos += maxBlock)
    {
        const int n = juce::jmin (maxBlock, numSamples - pos);
        auto block = whole.getSubBlock ((size_t) pos, (size_t) n);
        for (int c = 0; c < numCh; ++c)
        {
            const auto r = juce::FloatVectorOperations::findMinAndMax (block.getChannelPointer ((size_t) c), n);
            inPk = std::max ({ inPk, r.getEnd(), -r.getStart() });
        }

        auto up = oversampling.processSamplesUp (block);
        const int upN = (int) up.getNumSamples();
        float* ch[2] = { up.getChannelPointer (0), up.getChannelPointer (numCh > 1 ? 1 : 0) };

        for (int i = 0; i < upN; ++i)
        {
            sat.tick();
            const float dg   = driveS.getNextValue();
            const float comp = compS.getNextValue();
            const float bias = biasS.getNextValue();
            const float lo   = lowS.getNextValue();
            const float hi   = highS.getNextValue();
            const float byp  = bypassS.getNextValue();
            const float wet  = mixS.getNextValue() * (1.0f - byp);  // bypass = mix 0 through the same filters (latency stays)
            const float out  = outS.getNextValue();
            const float og   = out + (1.0f - out) * byp;

            for (int c = 0; c < numCh; ++c)
            {
                const float x = ch[c][i];
                heatPk = std::max (heatPk, std::abs (x * dg));
                ch[c][i] = sat.process (c, x, dg, bias, lo, hi, comp, wet) * og;
            }
        }

        oversampling.processSamplesDown (block);

        for (int c = 0; c < numCh; ++c)
        {
            const auto r = juce::FloatVectorOperations::findMinAndMax (block.getChannelPointer ((size_t) c), n);
            outPk = std::max ({ outPk, r.getEnd(), -r.getStart() });
        }
    }

    // safety net: a blown-up filter state must never stay stuck
    if (! std::isfinite (outPk))
    {
        sat.reset();
        oversampling.reset();
        buffer.clear();
        outPk = 0.0f;
    }

    PyreMeters::storeMax (meters.inPeak, inPk);
    PyreMeters::storeMax (meters.outPeak, outPk);
    PyreMeters::storeMax (meters.heat, t.bypass > 0.5f ? 0.0f : heatPk);
}

void PyreProcessor::setCurrentProgram (int index)
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

const juce::String PyreProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, presets::count) ? juce::String (presets::factory[index].name) : juce::String();
}

bool PyreProcessor::isPresetModified() const
{
    for (auto& [id, value] : presets::values (presets::factory[currentPreset]))
        if (auto* p = apvts.getParameter (id))
        {
            const float v = p->convertFrom0to1 (p->getValue());
            if (std::abs (v - value) > std::max (0.051f, std::abs (value) * 0.002f))
                return true;
        }
    return false;
}

void PyreProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PyreProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, presets::count - 1, (int) apvts.state.getProperty ("preset", 0));
        }
}

juce::AudioProcessorEditor* PyreProcessor::createEditor() { return new PyreEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PyreProcessor(); }
