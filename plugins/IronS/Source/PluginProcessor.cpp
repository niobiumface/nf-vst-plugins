#include "PluginProcessor.h"
#include "PluginEditor.h"

IronSProcessor::IronSProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "IronS", params::createLayout())
{
    pThresh = apvts.getRawParameterValue (params::thresh);
    pRange  = apvts.getRawParameterValue (params::range);
    pFreq   = apvts.getRawParameterValue (params::freq);
    pWidth  = apvts.getRawParameterValue (params::width);
    pSpeed  = apvts.getRawParameterValue (params::speed);
    pSmack  = apvts.getRawParameterValue (params::smack);
    pLook   = apvts.getRawParameterValue (params::look);
    pLink   = apvts.getRawParameterValue (params::link);
    pOut    = apvts.getRawParameterValue (params::output);
    pMode   = apvts.getRawParameterValue (params::mode);
    pDetect = apvts.getRawParameterValue (params::detect);
    pCut    = apvts.getRawParameterValue (params::cut);
    pListen = apvts.getRawParameterValue (params::listen);
    pBypass = apvts.getRawParameterValue (params::bypass);
    bypassParam = apvts.getParameter (params::bypass);
}

bool IronSProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void IronSProcessor::prepareToPlay (double sampleRate, int)
{
    deEsser.prepare (sampleRate);
    setLatencySamples (deEsser.getLatency()); // constant: the lookahead only moves the gain, never the audio

    outGain.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    bypassMix.reset (sampleRate, 0.02);
    bypassMix.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
}

void IronSProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
    if (numCh == 0) return;

    sdsp::DeEsserParams dp;
    dp.threshDb   = pThresh->load();
    dp.rangeDb    = pRange->load();
    dp.freqHz     = pFreq->load();
    dp.widthOct   = pWidth->load();
    dp.releaseMs  = pSpeed->load();
    dp.lookMs     = pLook->load();
    dp.smack      = pSmack->load() * 0.01f;
    dp.link       = pLink->load() * 0.01f;
    dp.relative   = pMode->load() < 0.5f;
    dp.bandDetect = pDetect->load() < 0.5f;
    dp.split      = pCut->load() > 0.5f;
    dp.listen     = juce::roundToInt (pListen->load());
    deEsser.setParams (dp);

    outGain.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    bypassMix.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    float* ch[2] = { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };
    const float norm = 1.0f / (float) numCh;
    float inPk = 0.0f, outPk = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float in[2] = { ch[0][i], ch[1][i] }, wet[2], dry[2];
        deEsser.processFrame (in, wet, dry, numCh);

        const float og = outGain.getNextValue();
        const float byp = bypassMix.getNextValue();
        float inSum = 0.0f, outSum = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            const float w = wet[c] * og;
            const float o = w + (dry[c] - w) * byp;
            ch[c][i] = o;
            inPk = std::max (inPk, std::abs (dry[c]));
            outPk = std::max (outPk, std::abs (o));
            inSum += dry[c]; outSum += o;
        }
        scope.push (inSum * norm, outSum * norm);
    }

    // safety net: a blown-up filter state must never stay stuck
    if (! std::isfinite (outPk))
    {
        deEsser.reset();
        buffer.clear();
        outPk = 0.0f;
    }

    const bool active = pBypass->load() < 0.5f;
    IronMeters::storeMax (meters.inPeak, inPk);
    IronMeters::storeMax (meters.outPeak, outPk);
    IronMeters::storeMax (meters.level, deEsser.getMeterLevelDb());
    IronMeters::storeMax (meters.reduction, active ? deEsser.getMeterReductionDb() : 0.0f);
    IronMeters::storeMax (meters.smack, active ? deEsser.getMeterSmackDb() : 0.0f);
    deEsser.resetMeters();
}

void IronSProcessor::setCurrentProgram (int index)
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

const juce::String IronSProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, presets::count) ? juce::String (presets::factory[index].name) : juce::String();
}

bool IronSProcessor::isPresetModified() const
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

void IronSProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void IronSProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, presets::count - 1, (int) apvts.state.getProperty ("preset", 0));
        }
}

juce::AudioProcessorEditor* IronSProcessor::createEditor() { return new IronSEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IronSProcessor(); }
