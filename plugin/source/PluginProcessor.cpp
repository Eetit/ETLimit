#include "PluginProcessor.h"
#include "PluginEditor.h"

ETLimitProcessor::ETLimitProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ETLimitState", createParameterLayout())
{
    inputGainValue = apvts.getRawParameterValue (ParamIDs::inputGain.getParamID());
    bypassValue = apvts.getRawParameterValue (ParamIDs::bypass.getParamID());
    bypassParam = apvts.getParameter (ParamIDs::bypass.getParamID());
    jassert (inputGainValue != nullptr && bypassValue != nullptr && bypassParam != nullptr);
}

bool ETLimitProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // A limiter must not change the channel count.
    return layouts.getMainInputChannelSet() == out;
}

void ETLimitProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Called by the host before playback starts (never during processBlock),
    // so allocating memory here is fine.
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (engine.getLatencySamples());
}

void ETLimitProcessor::reset()
{
    engine.reset();
}

void ETLimitProcessor::runEngine (juce::AudioBuffer<float>& buffer, bool forceBypass) noexcept
{
    // Flush tiny "denormal" numbers to zero. Without this, filters decaying
    // towards silence can make the CPU usage spike enormously.
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    etlimit::EngineSettings s;
    s.inputGainDb = inputGainValue->load (std::memory_order_relaxed);
    s.bypass = forceBypass || bypassValue->load (std::memory_order_relaxed) >= 0.5f;
    engine.setSettings (s);

    engine.process (buffer.getArrayOfWritePointers(), numOut, buffer.getNumSamples());
}

void ETLimitProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    runEngine (buffer, false);
}

void ETLimitProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Some hosts bypass the plugin without using our bypass parameter. We
    // still run the engine so the (latency-compensated) dry path is used and
    // the transition is crossfaded instead of clicking.
    runEngine (buffer, true);
}

void ETLimitProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ETLimitProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ETLimitProcessor::createEditor()
{
    return new ETLimitEditor (*this);
}

// This function is how the plugin format wrapper creates our processor.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ETLimitProcessor();
}
