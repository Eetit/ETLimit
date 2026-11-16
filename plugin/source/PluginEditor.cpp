#include "PluginEditor.h"
#include "PluginProcessor.h"

ETLimitEditor::ETLimitEditor (ETLimitProcessor& p)
    : AudioProcessorEditor (p), generic (p)
{
    addAndMakeVisible (generic);
    setSize (420, 160);
}

void ETLimitEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1f24));
}

void ETLimitEditor::resized()
{
    generic.setBounds (getLocalBounds());
}
