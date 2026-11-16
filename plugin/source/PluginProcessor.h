#pragma once

#include "Parameters.h"

#include <etlimit/Engine.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
    The JUCE "AudioProcessor" is the object the host (DAW) talks to.

    Its jobs are deliberately small:
      - declare which channel layouts we support (mono, stereo)
      - own the parameters and save/restore them with the project
      - read parameter values at the start of each block and hand them to
        the DSP engine
      - report latency to the host

    All actual audio processing lives in etlimit::Engine (core/), which has
    no JUCE dependency and is tested on its own.
*/
class ETLimitProcessor final : public juce::AudioProcessor
{
public:
    ETLimitProcessor();
    ~ETLimitProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }

private:
    void runEngine (juce::AudioBuffer<float>& buffer, bool forceBypass) noexcept;

    juce::AudioProcessorValueTreeState apvts;

    // Cached raw pointers to the parameters' atomic values: reading these on
    // the audio thread is lock-free.
    std::atomic<float>* inputGainValue = nullptr;
    std::atomic<float>* bypassValue = nullptr;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    etlimit::Engine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ETLimitProcessor)
};
