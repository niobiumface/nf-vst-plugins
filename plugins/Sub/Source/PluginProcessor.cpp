#include "PluginProcessor.h"
#include "PluginEditor.h"

SubProcessor::SubProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Sub", params::createLayout())
{
    pCutoff = apvts.getRawParameterValue (params::cutoff);
    pReso   = apvts.getRawParameterValue (params::reso);
    pLayers = apvts.getRawParameterValue (params::layers);
    pDrive  = apvts.getRawParameterValue (params::drive);
    pMix    = apvts.getRawParameterValue (params::mix);
    pOut    = apvts.getRawParameterValue (params::output);
}

bool SubProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SubProcessor::prepareToPlay (double sampleRate, int)
{
    filter.prepare (sampleRate);

    cutoffS.reset (sampleRate, 0.03);  cutoffS.setCurrentAndTargetValue (pCutoff->load());
    resoS.reset (sampleRate, 0.03);    resoS.setCurrentAndTargetValue (pReso->load() * 0.01f);
    layerS.reset (sampleRate, 0.06);   layerS.setCurrentAndTargetValue (pLayers->load());
    driveS.reset (sampleRate, 0.03);   driveS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    driveWetS.reset (sampleRate, 0.03); driveWetS.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, pDrive->load() / 3.0f));
    mixS.reset (sampleRate, 0.03);     mixS.setCurrentAndTargetValue (pMix->load() * 0.01f);
    outS.reset (sampleRate, 0.03);     outS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
}

void SubProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
    if (numCh == 0) return;

    cutoffS.setTargetValue (pCutoff->load());
    resoS.setTargetValue (pReso->load() * 0.01f);
    layerS.setTargetValue (pLayers->load());
    driveS.setTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    driveWetS.setTargetValue (juce::jlimit (0.0f, 1.0f, pDrive->load() / 3.0f));
    mixS.setTargetValue (pMix->load() * 0.01f);
    outS.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    float* ch[2] = { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };
    float inPk = 0.0f, outPk = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        filter.setParams (cutoffS.getNextValue(), resoS.getNextValue());
        const float lp  = layerS.getNextValue();
        const float dg  = driveS.getNextValue();
        const float dw  = driveWetS.getNextValue();
        const float wet = mixS.getNextValue();
        const float og  = outS.getNextValue();

        for (int c = 0; c < numCh; ++c)
        {
            const float x = ch[c][i];
            inPk = std::max (inPk, std::abs (x));
            const float y = filter.process (c, sdsp::driveSaturate (x, dg, dw), lp);
            const float o = (x + (y - x) * wet) * og;
            ch[c][i] = o;
            outPk = std::max (outPk, std::abs (o));
        }
    }

    // safety net: a blown-up filter state must never stay stuck
    if (! std::isfinite (outPk))
    {
        filter.reset();
        buffer.clear();
        outPk = 0.0f;
    }

    SubMeters::storeMax (meters.inPeak, inPk);
    SubMeters::storeMax (meters.outPeak, outPk);
}

void SubProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SubProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SubProcessor::createEditor() { return new SubEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SubProcessor(); }
