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

    // Resize the workbench buffer (no audio processing here, just memory allocation)
    doubleBuffer.setSize(getMainBusNumOutputChannels(), samplesPerBlock);
    doubleBuffer.clear(); // Ensure it starts at zero!

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
        doubleBuffer.clear(i, 0, doubleBuffer.getNumSamples());
    }

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    updateCoefficients(getSampleRate());

    // -----------------------------------------------------------
    // 1. UPSample to 64-bit (Float -> Double)
    // -----------------------------------------------------------
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* floatRead = buffer.getReadPointer(ch);
        auto* doubleWrite = doubleBuffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            doubleWrite[i] = static_cast<double>(floatRead[i]);
        }
    }

    // 1. Check if the buffer is silent
    if (buffer.getMagnitude(0, numSamples) < 0.000001f) // Roughly -120dB
    {
        // If the buffer is silent, we might still be 'ringing'
        // For a simple demo, you can check if we've been silent for a few blocks
        if (++silentBlockCount > 100)
        {
            return; // SKIP THE FILTER MATH
        }
    }
    else
    {
        silentBlockCount = 0;
    }

    juce::dsp::AudioBlock<double> doubleBlock(doubleBuffer.getArrayOfWritePointers(),
        doubleBuffer.getNumChannels(),
        numSamples);

    auto hpIsBypassed = parameters.getRawParameterValue("bypassHp")->load();
    auto lpIsBypassed = parameters.getRawParameterValue("bypassLp")->load();

    if (!hpIsBypassed) highPass.process(juce::dsp::ProcessContextReplacing<double>(doubleBlock));
    
    if (!lpIsBypassed) lowPass.process(juce::dsp::ProcessContextReplacing<double>(doubleBlock));

    // 3. Cast back to 32-bit (Double -> Float) for the DAW
    // -----------------------------------------------------------
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* doubleRead = doubleBuffer.getReadPointer(ch);
        auto* floatWrite = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            floatWrite[i] = static_cast<float>(doubleRead[i]);
        }
    }
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

	int M = filterOrder+1;
    
    double wcHP = 2.0 * juce::MathConstants<double>::pi * hpCutoffDouble / sampleRate;
    double wcLP = 2.0 * juce::MathConstants<double>::pi * lpCutoffDouble / sampleRate;

    static constexpr int maxM = 251;
    std::vector<double> hHP(maxM, 0.0);
    std::vector<double> hLP(maxM, 0.0);

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
        
        if (std::abs(n - delay) < 1e-9)
        {
            hHP.at(n) = (1.0 - (wcHP / juce::MathConstants<double>::pi)) * window;
            hLP.at(n) = (wcLP / juce::MathConstants<double>::pi) * window;
        }
        else
        {
            hHP.at(n) = -std::sin(wcHP * (n - delay)) / (juce::MathConstants<double>::pi * (n - delay)) * window;
            hLP.at(n) = std::sin(wcLP * (n - delay)) / (juce::MathConstants<double>::pi * (n - delay)) * window;
        }
    }

    *highPass.state = juce::dsp::FIR::Coefficients<double>::Coefficients(&hHP[0], hHP.size());
    *lowPass.state = juce::dsp::FIR::Coefficients<double>::Coefficients(&hLP[0], hLP.size());

    /*auto& hpConvolution = filter.template get<0>();
    hpConvolution.loadImpulseResponse(hHP.data(), maxM,
        juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::yes,
        M,
        juce::dsp::Convolution::Normalise::yes);

    auto& lpConvolution = filter.template get<1>();
    lpConvolution.loadImpulseResponse(hLP.data(), maxM,
        juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::yes,
        M,
        juce::dsp::Convolution::Normalise::yes);*/
}

juce::AudioProcessorValueTreeState::ParameterLayout FIRFilterAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hpCutoff", "High Pass Cutoff Frequency", juce::NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 10.f, juce::AudioParameterFloatAttributes()));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("lpCutoff", "Low Pass Cutoff Frequency", juce::NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 20000.f, juce::AudioParameterFloatAttributes()));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterOrder",
        "Filter Order",
		juce::NormalisableRange<float>(10.f, 250.f, 10.f),
		10, juce::AudioParameterFloatAttributes().withStringFromValueFunction([](int value, int) { return std::to_string(value+1) + " taps"; })
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
