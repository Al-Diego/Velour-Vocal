#include "PluginProcessor.h"
#include "UI/PluginEditor.h"

VelourVocalProcessor::VelourVocalProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VelourVocal", Params::createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    raw.amount  = { get (ParamIDs::lowAmount),  get (ParamIDs::midAmount),  get (ParamIDs::highAmount) };
    raw.enabled = { get (ParamIDs::lowEnabled), get (ParamIDs::midEnabled), get (ParamIDs::highEnabled) };
    raw.solo    = { get (ParamIDs::lowSolo),    get (ParamIDs::midSolo),    get (ParamIDs::highSolo) };
    raw.xoverLowMid  = get (ParamIDs::xoverLowMid);
    raw.xoverMidHigh = get (ParamIDs::xoverMidHigh);
    raw.highShape    = get (ParamIDs::highShape);
    raw.voiceProfile = get (ParamIDs::voiceProfile);
    raw.dynThreshold = get (ParamIDs::dynThreshold);
    raw.dynEnabled   = get (ParamIDs::dynEnabled);
    raw.outputGain   = get (ParamIDs::outputGain);
    raw.delta        = get (ParamIDs::delta);
}

bool VelourVocalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VelourVocalProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    engine.setSettings (readSettings (false));
    engine.prepare (sampleRate, std::max (getTotalNumInputChannels(), getTotalNumOutputChannels()));
    setLatencySamples (engine.getLatencySamples());

    outputGain.reset (sampleRate, 0.05);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw.outputGain->load()));
}

double VelourVocalProcessor::getTailLengthSeconds() const
{
    return getSampleRate() > 0.0 ? engine.getLatencySamples() / getSampleRate() : 0.0;
}

SpectralSuppressor::Settings VelourVocalProcessor::readSettings (bool bypassed) const noexcept
{
    SpectralSuppressor::Settings s;
    for (size_t b = 0; b < 3; ++b)
    {
        s.amount[b]  = raw.amount[b]->load() * 0.01f;
        s.enabled[b] = raw.enabled[b]->load() > 0.5f;
        s.solo[b]    = raw.solo[b]->load() > 0.5f;
    }
    s.xoverLowMid    = raw.xoverLowMid->load();
    s.xoverMidHigh   = raw.xoverMidHigh->load();
    s.highBroad      = raw.highShape->load() > 0.5f;
    s.profile        = juce::roundToInt (raw.voiceProfile->load());
    s.dynEnabled     = raw.dynEnabled->load() > 0.5f;
    s.dynThresholdDb = raw.dynThreshold->load();
    s.delta          = raw.delta->load() > 0.5f;
    s.bypass         = bypassed;
    return s;
}

void VelourVocalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process (buffer, false);
}

// Hosts call this when the plugin is bypassed. We keep running the STFT so the
// latency stays constant and switching bypass on/off is click-free.
void VelourVocalProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process (buffer, true);
}

void VelourVocalProcessor::process (juce::AudioBuffer<float>& buffer, bool bypassed) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    engine.setSettings (readSettings (bypassed));
    engine.process (buffer.getArrayOfWritePointers(), std::min (numOut, buffer.getNumChannels()), numSamples);

    outputGain.setTargetValue (bypassed ? 1.0f : juce::Decibels::decibelsToGain (raw.outputGain->load()));

    if (outputGain.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outputGain.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        buffer.applyGain (outputGain.getTargetValue());
    }
}

juce::AudioProcessorEditor* VelourVocalProcessor::createEditor()
{
    return new VelourVocalEditor (*this);
}

void VelourVocalProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VelourVocalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VelourVocalProcessor();
}
