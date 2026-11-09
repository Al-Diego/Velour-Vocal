#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/SpectralSuppressor.h"
#include "Parameters.h"

class VelourVocalProcessor : public juce::AudioProcessor
{
public:
    VelourVocalProcessor();
    ~VelourVocalProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return JucePlugin_Name; }
    bool acceptsMidi() const override                   { return false; }
    bool producesMidi() const override                  { return false; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override                       { return Params::numFactoryPresets; }
    int getCurrentProgram() override                    { return currentPreset.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    SpectralSuppressor& getEngine() noexcept { return engine; }

private:
    void process (juce::AudioBuffer<float>&, bool bypassed) noexcept;
    SpectralSuppressor::Settings readSettings (bool bypassed) const noexcept;

    SpectralSuppressor engine;
    std::atomic<int> currentPreset { 0 };
    juce::SmoothedValue<float> outputGain;

    struct RawParams
    {
        std::array<std::atomic<float>*, 3> amount {}, enabled {}, solo {};
        std::atomic<float>* xoverLowMid = nullptr;
        std::atomic<float>* xoverMidHigh = nullptr;
        std::atomic<float>* highShape = nullptr;
        std::atomic<float>* voiceProfile = nullptr;
        std::atomic<float>* dynThreshold = nullptr;
        std::atomic<float>* dynEnabled = nullptr;
        std::atomic<float>* outputGain = nullptr;
        std::atomic<float>* delta = nullptr;
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelourVocalProcessor)
};
