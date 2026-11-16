#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
    Every automatable parameter is declared here.

    The string IDs are what DAWs store in projects and automation lanes, so
    once released they must never be renamed. The version number in each
    juce::ParameterID records in which plugin version a parameter appeared.

    A parameter is only added once the DSP behind it actually works, so the
    plugin never shows a control that does nothing.
*/
namespace ParamIDs
{
inline const juce::ParameterID inputGain { "inputGain", 1 };
inline const juce::ParameterID bypass { "bypass", 1 };
} // namespace ParamIDs

namespace ParamRanges
{
constexpr float inputGainMinDb = -12.0f;
constexpr float inputGainMaxDb = 36.0f;
} // namespace ParamRanges

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
