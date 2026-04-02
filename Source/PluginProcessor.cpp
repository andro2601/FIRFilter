/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FIRFilterAudioProcessor::FIRFilterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
#endif
    parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

FIRFilterAudioProcessor::~FIRFilterAudioProcessor()
{
}

//==============================================================================
const juce::String FIRFilterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FIRFilterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FIRFilterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FIRFilterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FIRFilterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FIRFilterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int FIRFilterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FIRFilterAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String FIRFilterAudioProcessor::getProgramName (int index)
{
    return {};
}

void FIRFilterAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void FIRFilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getMainBusNumOutputChannels();

    highPass.prepare(spec);
    highPass.reset();
    lowPass.prepare(spec);
    lowPass.reset();

    updateCoefficients(sampleRate);
}

void FIRFilterAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FIRFilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void FIRFilterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    updateCoefficients(getSampleRate());

    // Check if the buffer is silent
    if (buffer.getMagnitude(0, numSamples) < 0.000001f) // Roughly -120dB
    {
        // If the buffer is silent, we might still be 'ringing'
        if (++silentBlockCount > 100)
        {
            return; // skip the filter math
        }
    }
    else
    {
        silentBlockCount = 0;
    }

    juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),
        buffer.getNumChannels(),
        numSamples);

    auto hpIsBypassed = parameters.getRawParameterValue("bypassHp")->load();
    auto lpIsBypassed = parameters.getRawParameterValue("bypassLp")->load();

    if (!hpIsBypassed) highPass.process(juce::dsp::ProcessContextReplacing<float>(block));
    
    if (!lpIsBypassed) lowPass.process(juce::dsp::ProcessContextReplacing<float>(block));
}

void FIRFilterAudioProcessor::updateCoefficients(double sampleRate) {
    float hpCutoff = parameters.getRawParameterValue("hpCutoff")->load();
    float lpCutoff = parameters.getRawParameterValue("lpCutoff")->load();
	int filterOrder = static_cast<int>(parameters.getRawParameterValue("filterOrder")->load());
	int windowType = static_cast<int>(parameters.getRawParameterValue("window")->load());
	float kaiserAlpha = parameters.getRawParameterValue("kaiserAlpha")->load();

    if (hpCutoff == lastHpCutoff && lpCutoff == lastLpCutoff && filterOrder == lastFilterOrder && windowType == lastWindow && kaiserAlpha == lastKaiserAlpha) return;
    
    lastHpCutoff = hpCutoff;
    lastLpCutoff = lpCutoff;
	lastFilterOrder = filterOrder;
	lastWindow = windowType;
	lastKaiserAlpha = kaiserAlpha;

	double hpCutoffDouble = static_cast<double>(hpCutoff);
	double lpCutoffDouble = static_cast<double>(lpCutoff);
	double kaiserAlphaDouble = static_cast<double>(kaiserAlpha);

	int M = std::pow(2,filterOrder+7)-1;

    hpCoeffs.resize(M);
    lpCoeffs.resize(M);
    
    double wcHP = 2.0 * juce::MathConstants<double>::pi * hpCutoffDouble / sampleRate;
    double wcLP = 2.0 * juce::MathConstants<double>::pi * lpCutoffDouble / sampleRate;

    for (int n = 0; n < M; ++n)
    {
        double delay = (M - 1) / 2.0;
		double window = 1.0; // Default to rectangular window
        switch (windowType) {
            case 0: // Blackman
				window = 0.42 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * n / (M - 1.0)) + 0.08 * std::cos(4.0 * juce::MathConstants<double>::pi * n / (M - 1.0));
                break;
			case 1: // Hamming
				window = 0.54 - 0.46 * std::cos(2.0 * juce::MathConstants<double>::pi * n / (M - 1.0));
                break;
			case 2: // Hann
				window = 0.5 * (1.0 - std::cos(2.0 * juce::MathConstants<double>::pi * n / (M - 1.0)));
                break;
            case 3: // Kaiser
            {
                double bes = std::cyl_bessel_i(0, juce::MathConstants<double>::pi * kaiserAlpha);
                double besN = std::cyl_bessel_i(0, juce::MathConstants<double>::pi * kaiserAlpha * std::sqrt(1.0 - std::pow((2.0 * n) / (M - 1.0) - 1.0, 2)));
                window = besN / bes;
                break;
			}
			default: // Rectangular
				window = 1.0;
				break;
        }
        
        if (std::abs(n - delay) < 1e-9) // centralni član
        {
            hpCoeffs.at(n) = (1.0 - (wcHP / juce::MathConstants<double>::pi)) * window;
            lpCoeffs.at(n) = (wcLP / juce::MathConstants<double>::pi) * window;
        }
        else
        {
            hpCoeffs.at(n) = -std::sin(wcHP * (n - delay)) / (juce::MathConstants<double>::pi * (n - delay)) * window;
            lpCoeffs.at(n) = std::sin(wcLP * (n - delay)) / (juce::MathConstants<double>::pi * (n - delay)) * window;
        }
    }

    // 1. Wrap your raw data in a temporary AudioBuffer
    // (1 channel, M samples)
    juce::AudioBuffer<float> irHpBuffer(1, (int)M);
    juce::AudioBuffer<float> irLpBuffer(1, (int)M);

    // 2. Copy your coefficients into the buffer
    irHpBuffer.copyFrom(0, 0, hpCoeffs.data(), (int)M);
    irLpBuffer.copyFrom(0, 0, lpCoeffs.data(), (int)M);

    highPass.loadImpulseResponse(
        std::move(irHpBuffer),
        getSampleRate(),
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::no,
        juce::dsp::Convolution::Normalise::no
    );

    lowPass.loadImpulseResponse(
        std::move(irLpBuffer),
        getSampleRate(),
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::no,
        juce::dsp::Convolution::Normalise::no
    );
}

juce::AudioProcessorValueTreeState::ParameterLayout FIRFilterAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hpCutoff", "High Pass Cutoff Frequency", juce::NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 10.f, juce::AudioParameterFloatAttributes()));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("lpCutoff", "Low Pass Cutoff Frequency", juce::NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 20000.f, juce::AudioParameterFloatAttributes()));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        "filterOrder",
        "Filter Order",
        juce::StringArray{ "126", "254", "510", "1022", "2046" },
        0
    ));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        "window",
        "Windowing Function",
        juce::StringArray{ "Blackman", "Hamming", "Hann", "Kaiser", "Rectangular"},
        0
    ));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        "kaiserAlpha",
        "Kaiser Alpha",
        juce::NormalisableRange<float>(0.f, 10.f, 0.1f),
        2.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int) { return "α = " + std::to_string(value); })
    ));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("bypassHp", "Bypass HP", false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("bypassLp", "Bypass LP", false));

    return { parameters.begin(), parameters.end() };
}

//==============================================================================
bool FIRFilterAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* FIRFilterAudioProcessor::createEditor()
{
    return new FIRFilterAudioProcessorEditor (*this);
}

//==============================================================================
void FIRFilterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // 1. Create an XML element to hold your data
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // 2. Convert that XML to a binary block for the DAW
    copyXmlToBinary(*xml, destData);
}

void FIRFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // 1. Convert the binary block back into XML
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        // 2. Check if the XML tag matches your ValueTree name
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            // 3. Update the APVTS, which automatically updates your sliders
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));

            // 4. IMPORTANT: Manually trigger your filter update!
            updateCoefficients(getSampleRate());
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FIRFilterAudioProcessor();
}
