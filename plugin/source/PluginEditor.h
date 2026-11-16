#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class ETLimitProcessor;

/**
    Temporary development interface: JUCE's generic editor draws one slider
    or button per parameter. The real mastering interface is built in
    stage 8, once the DSP and metering are finished and verified.
*/
class ETLimitEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ETLimitEditor (ETLimitProcessor&);
    ~ETLimitEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ETLimitEditor)
};
