#include "Parameters.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbString = [] (float v, int) { return juce::String (v, 2) + " dB"; };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::inputGain,
        "Input Gain",
        juce::NormalisableRange<float> (ParamRanges::inputGainMinDb, ParamRanges::inputGainMaxDb, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbString)));

    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::bypass, "Bypass", false));

    return layout;
}
