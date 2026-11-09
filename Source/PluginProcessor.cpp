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

const juce::String VelourVocalProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, Params::numFactoryPresets) ? Params::factoryPresets[index].name : "";
}

void VelourVocalProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, Params::numFactoryPresets))
        return;

    currentPreset = index;
    apvts.state.setProperty ("preset", index, nullptr);

    const auto& p = Params::factoryPresets[index];
    const auto xovers = Params::defaultsForProfile (juce::roundToInt (raw.voiceProfile->load()));

    auto set = [this] (const char* id, float value)
    {
        auto* param = apvts.getParameter (id);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    };

    set (ParamIDs::lowAmount,    p.low);
    set (ParamIDs::midAmount,    p.mid);
    set (ParamIDs::highAmount,   p.high);
    set (ParamIDs::highShape,    p.highBroad ? 1.0f : 0.0f);
    set (ParamIDs::dynEnabled,   p.dynOn ? 1.0f : 0.0f);
    set (ParamIDs::dynThreshold, p.dynThresholdDb);
    set (ParamIDs::xoverLowMid,  xovers.xoverLowMid);
    set (ParamIDs::xoverMidHigh, xovers.xoverMidHigh);
    set (ParamIDs::outputGain,   0.0f);
    set (ParamIDs::delta,        0.0f);
    for (auto* id : { ParamIDs::lowEnabled, ParamIDs::midEnabled, ParamIDs::highEnabled })
        set (id, 1.0f);
    for (auto* id : { ParamIDs::lowSolo, ParamIDs::midSolo, ParamIDs::highSolo })
        set (id, 0.0f);
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
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, Params::numFactoryPresets - 1, (int) apvts.state.getProperty ("preset", 0));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VelourVocalProcessor();
}
