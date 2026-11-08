#include "PluginProcessor.h"
#include "PluginEditor.h"

CuttaProcessor::CuttaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CUtTA", params::createLayout())
{
    pGateOn  = apvts.getRawParameterValue (params::gateOn);
    pThresh  = apvts.getRawParameterValue (params::gateThresh);
    pRange   = apvts.getRawParameterValue (params::gateRange);
    pAttack  = apvts.getRawParameterValue (params::gateAttack);
    pHold    = apvts.getRawParameterValue (params::gateHold);
    pRelease = apvts.getRawParameterValue (params::gateRelease);
    pLimOn   = apvts.getRawParameterValue (params::limOn);
    pDrive   = apvts.getRawParameterValue (params::limDrive);
    pCeiling = apvts.getRawParameterValue (params::limCeiling);
    pLimRel  = apvts.getRawParameterValue (params::limRelease);
}

bool CuttaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void CuttaProcessor::prepareToPlay (double sampleRate, int)
{
    gate.prepare (sampleRate);
    limiter.prepare (sampleRate, 1.5f);
    driveGain.reset (sampleRate, 0.03);
    driveGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    setLatencySamples (limiter.getLatency()); // constant, also when the limiter is bypassed
}

void CuttaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
    if (numCh == 0) return;

    const bool gateOn = pGateOn->load() > 0.5f;
    const bool limOn  = pLimOn->load()  > 0.5f;
    gate.setParams (pThresh->load(), pRange->load(), pAttack->load(), pHold->load(), pRelease->load());
    limiter.setParams (pCeiling->load(), pLimRel->load());
    driveGain.setTargetValue (limOn ? juce::Decibels::decibelsToGain (pDrive->load()) : 1.0f);

    float* ch[2] = { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };

    float inPk = 0.0f, prePk = 0.0f, outPk = 0.0f, minLim = 1.0f, g = 1.0f;
    float frame[2];

    for (int i = 0; i < numSamples; ++i)
    {
        float det = 0.0f;
        for (int c = 0; c < numCh; ++c) det = std::max (det, std::abs (ch[c][i]));
        inPk = std::max (inPk, det);

        g = gate.process (det);
        const float gGain = gateOn ? g : 1.0f;
        const float drv = driveGain.getNextValue();

        float pk = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            frame[c] = ch[c][i] * gGain * drv;
            pk = std::max (pk, std::abs (frame[c]));
        }
        prePk = std::max (prePk, pk);

        minLim = std::min (minLim, limiter.processFrame (frame, numCh, limOn));

        for (int c = 0; c < numCh; ++c)
        {
            ch[c][i] = frame[c];
            outPk = std::max (outPk, std::abs (frame[c]));
        }
    }

    CuttaMeters::storeMax (meters.inPeak,  inPk);
    CuttaMeters::storeMax (meters.preLim,  prePk);
    CuttaMeters::storeMax (meters.outPeak, outPk);
    CuttaMeters::storeMin (meters.limGain, minLim);
    meters.gateGain.store (gateOn ? g : 1.0f, std::memory_order_relaxed);
    meters.gateOpen.store (gateOn ? gate.isOpen() : true, std::memory_order_relaxed);
}

void CuttaProcessor::setCurrentProgram (int index)
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

const juce::String CuttaProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, presets::count) ? juce::String (presets::factory[index].name) : juce::String();
}

bool CuttaProcessor::isPresetModified() const
{
    for (auto& [id, value] : presets::values (presets::factory[currentPreset]))
        if (auto* p = apvts.getParameter (id))
            if (std::abs (p->convertFrom0to1 (p->getValue()) - value) > 0.051f)
                return true;
    return false;
}

void CuttaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void CuttaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, presets::count - 1, (int) apvts.state.getProperty ("preset", 0));
        }
}

juce::AudioProcessorEditor* CuttaProcessor::createEditor() { return new CuttaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CuttaProcessor(); }
