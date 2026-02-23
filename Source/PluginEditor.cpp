/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
FIRFilterAudioProcessorEditor::FIRFilterAudioProcessorEditor (FIRFilterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // High-Pass Filter Slider
    hpCutoffSlider.setSliderStyle(Slider::Rotary);
    hpCutoffSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(hpCutoffSlider);

    hpLabel.setText("High-Pass Cutoff", dontSendNotification);
    //hpLabel.attachToComponent(&hpCutoffSlider, false);
    addAndMakeVisible(hpLabel);

    // Low-Pass Filter Slider
    lpCutoffSlider.setSliderStyle(Slider::Rotary);
    lpCutoffSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(lpCutoffSlider);

    lpLabel.setText("Low-Pass Cutoff", dontSendNotification);
    //lpLabel.attachToComponent(&lpCutoffSlider, false);
    addAndMakeVisible(lpLabel);

    // Filter Order Slider
    filterOrderSlider.setSliderStyle(Slider::Rotary);
    filterOrderSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(filterOrderSlider);

	filterOrderLabel.setText("Filter Order", dontSendNotification);
	//filterOrderLabel.attachToComponent(&filterOrderSlider, false);
    addAndMakeVisible(filterOrderLabel);

	// Window Type ComboBox
	windowTypeComboBox.setJustificationType(Justification::centred);
	addAndMakeVisible(windowTypeComboBox);

	// Kaiser Alpha Slider
    kaiserAlphaSlider.setSliderStyle(Slider::Rotary);
    kaiserAlphaSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(kaiserAlphaSlider);

	kaiserAlphaLabel.setText("Kaiser Alpha", dontSendNotification);
	//kaiserAlphaLabel.attachToComponent(&kaiserAlphaSlider, true);
	addAndMakeVisible(kaiserAlphaLabel);

    // Initial visibility check
    kaiserAlphaSlider.setVisible(false);
    kaiserAlphaLabel.setVisible(false);

    // Bypass buttons
    addAndMakeVisible(bypassHpButton);
    bypassHpButton.setButtonText("Bypass HP");
    addAndMakeVisible(bypassLpButton);
    bypassLpButton.setButtonText("Bypass LP");
    
    // Trigger resized() whenever the combo box changes
    windowTypeComboBox.onChange = [this]
        {
            bool isKaiser = (windowTypeComboBox.getSelectedId() == 4); // "Kaiser" is index 3, ID 4
            kaiserAlphaSlider.setVisible(isKaiser);
            kaiserAlphaLabel.setVisible(isKaiser);
            resized();
        };

    // Attach sliders to parameters

    hpCutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "hpCutoff", hpCutoffSlider);

    lpCutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "lpCutoff", lpCutoffSlider);

    filterOrderAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "filterOrder", filterOrderSlider);

    windowTypeComboBox.addItemList(audioProcessor.parameters.getParameter("window")->getAllValueStrings(), 1);
    windowTypeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "window", windowTypeComboBox);

    kaiserAlphaAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
		audioProcessor.parameters, "kaiserAlpha", kaiserAlphaSlider);

    bypassHpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "bypassHp", bypassHpButton);

    bypassLpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "bypassLp", bypassLpButton);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 500);
}

FIRFilterAudioProcessorEditor::~FIRFilterAudioProcessorEditor()
{
}

//==============================================================================
void FIRFilterAudioProcessorEditor::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    g.setColour (Colours::white);
    g.setFont (FontOptions (15.0f));
    g.drawFittedText ("FIR Filter", getLocalBounds(), Justification::centredTop, 1);
}

void FIRFilterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20); // 20px padding around the edge
    auto mainRowHeight = 120;
    auto smallRowHeight = 80;
    auto labelWidth = 100;
    auto bypassHeight = 25; // Height for the bypass toggle

    // 1. High Pass (Large)
    auto hpArea = area.removeFromTop(mainRowHeight);
    auto hpControlArea = hpArea.removeFromLeft(labelWidth);
    hpLabel.setBounds(hpArea.removeFromLeft(labelWidth));
    bypassHpButton.setBounds(hpControlArea.removeFromTop(bypassHeight));
    hpCutoffSlider.setBounds(hpArea);

    // 2. Low Pass (Large)
    auto lpArea = area.removeFromTop(mainRowHeight);
    auto lpControlArea = lpArea.removeFromLeft(labelWidth);
    lpLabel.setBounds(lpArea.removeFromLeft(labelWidth));
    bypassLpButton.setBounds(lpControlArea.removeFromTop(bypassHeight));
    lpCutoffSlider.setBounds(lpArea);

    area.removeFromTop(20); // Gap

    // 3. ComboBox
    windowTypeComboBox.setBounds(area.removeFromTop(30));

    area.removeFromTop(20); // Gap

    // 4. Filter Order (Smaller)
    auto orderArea = area.removeFromTop(smallRowHeight);
    filterOrderLabel.setBounds(orderArea.removeFromLeft(labelWidth));
    filterOrderSlider.setBounds(orderArea);

    // 5. Kaiser Alpha (Conditional & Smaller)
    if (kaiserAlphaSlider.isVisible())
    {
        auto kaiserArea = area.removeFromTop(smallRowHeight);
        kaiserAlphaLabel.setBounds(kaiserArea.removeFromLeft(labelWidth));
        kaiserAlphaSlider.setBounds(kaiserArea);
    }
}
