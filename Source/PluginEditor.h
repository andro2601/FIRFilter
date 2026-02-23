/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class FIRFilterAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    FIRFilterAudioProcessorEditor (FIRFilterAudioProcessor&);
    ~FIRFilterAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FIRFilterAudioProcessor& audioProcessor;

    // Parameter Controls
    juce::Slider hpCutoffSlider;
    juce::Slider lpCutoffSlider;
	juce::Slider filterOrderSlider;
	juce::ComboBox windowTypeComboBox;
	juce::Slider kaiserAlphaSlider;
    juce::ToggleButton bypassHpButton;
    juce::ToggleButton bypassLpButton;
    

    // Labels
    juce::Label hpLabel;
    juce::Label lpLabel;
	juce::Label filterOrderLabel;
	juce::Label kaiserAlphaLabel;

    // Attachments to sync GUI with parameters
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpCutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpCutoffAttachment;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterOrderAttachment;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> windowTypeAttachment;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> kaiserAlphaAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassHpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassLpAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FIRFilterAudioProcessorEditor)
};
