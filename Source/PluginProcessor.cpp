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
void FIRFilterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getMainBusNumOutputChannels();

    conv.prepare(spec);
    conv.reset();

    // Red 14 znači 2^14 = 16384 uzoraka. Ovo daje vrhunsku frekvencijsku rezoluciju.
    int orderFFT = 14;
    fftSize = 1 << orderFFT;

    fsmFFT = std::make_unique<juce::dsp::FFT>(orderFFT);

    // Alociramo buffere za kompleksne brojeve
    fftData.resize(fftSize, { 0.0f, 0.0f });
    timeData.resize(fftSize, { 0.0f, 0.0f });

    /*
    lowPassChain.setBypassed<0>(false);
    lowPassChain.setBypassed<1>(true);
    lowPassChain.setBypassed<2>(true);
    lowPassChain.setBypassed<3>(true);
    highPassChain.setBypassed<0>(false);
    highPassChain.setBypassed<1>(true);
    highPassChain.setBypassed<2>(true);
    highPassChain.setBypassed<3>(true);
    */

    updateCoefficients(sampleRate);

    auto hpIsBypassed = parameters.getRawParameterValue("bypassHp")->load();
    auto lpIsBypassed = parameters.getRawParameterValue("bypassLp")->load();
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

    updateCoefficients(getSampleRate());

    conv.process(juce::dsp::ProcessContextReplacing<float>(block));
}

void FIRFilterAudioProcessor::updateCoefficients(double sampleRate) {
    // 1. Ako sample rate nije važeći ili FFT objekt nije stvoren, bježi van!
    if (sampleRate <= 0.0 || fsmFFT == nullptr) return;

    float lpCutoff = parameters.getRawParameterValue("lpCutoff")->load();
    float hpCutoff = parameters.getRawParameterValue("hpCutoff")->load();
	int filterOrder = static_cast<int>(parameters.getRawParameterValue("filterOrder")->load());
	int windowType = static_cast<int>(parameters.getRawParameterValue("window")->load());
	float kaiserAlpha = parameters.getRawParameterValue("kaiserAlpha")->load();
	int filterType = static_cast<int>(parameters.getRawParameterValue("filterType")->load());
    auto hpIsBypassed = parameters.getRawParameterValue("bypassHp")->load();
    auto lpIsBypassed = parameters.getRawParameterValue("bypassLp")->load();

    if (hpCutoff == lastHpCutoff && lpCutoff == lastLpCutoff && filterOrder == lastFilterOrder && windowType == lastWindow && kaiserAlpha == lastKaiserAlpha && filterType == lastFilterType && hpIsBypassed == hpLastBypassed && lpIsBypassed == lpLastBypassed) return;
    
	bool updateCoefficients = false;
	// ako je filter tip FSM (Butterworth), onda ćemo ažurirati koeficijente IIR-a samo ako su se cutoff frekvencije ili tip filtra promijenili
	if (filterType == 1 && !(hpCutoff == lastHpCutoff && lpCutoff == lastLpCutoff && filterType == lastFilterType && hpIsBypassed == hpLastBypassed && lpIsBypassed == lpLastBypassed)) {
		updateCoefficients = true;
	}

    int M = (1 << (filterOrder + 10)) + 1;

    if (lastFilterOrder != filterOrder) {
        coeffs.resize(M);
    }

    lastHpCutoff = hpCutoff;
    lastLpCutoff = lpCutoff;
	lastFilterOrder = filterOrder;
	lastWindow = windowType;
	lastKaiserAlpha = kaiserAlpha;
	lastFilterType = filterType;
    hpLastBypassed = hpIsBypassed;
    lpLastBypassed = lpIsBypassed;

	double hpCutoffDouble = static_cast<double>(hpCutoff);
	double lpCutoffDouble = static_cast<double>(lpCutoff);
	double kaiserAlphaDouble = static_cast<double>(kaiserAlpha);

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

        coeffs[n] = window;
    }

    if (filterType==1) // FSM (Butterworth)
    {
        if (updateCoefficients) {
            // Prolazimo samo kroz prvu polovicu FFT buffera (od 0 Hz do Nyquista)
			int N = 8; // Red Butterworth filtra (brzina pada)

            for (int k = 0; k <= fftSize / 2; ++k)
            {
                // Izračun stvarne frekvencije u Hz za trenutni FFT bin
                double f = k * sampleRate / static_cast<double>(fftSize);

                double magLP = 1.0;
                double magHP = 1.0;

                // 1. Ako NIJE bypassan, izračunaj stvarnu krivulju
                if (!lpIsBypassed) {
                    magLP = 1.0 / std::sqrt(1.0 + std::pow(f / lpCutoff, 2.0 * N));
                }

                // 2. Ako NIJE bypassan, izračunaj stvarnu krivulju
                if (!hpIsBypassed) {
                    // Poseban slučaj za 0Hz da izbjegnemo nan/inf
                    if (f < 1e-9) magHP = 0.0;
                    else magHP = 1.0 / std::sqrt(1.0 + std::pow(hpCutoff / f, 2.0 * N));
                }

                // 3. Pomnoži ih (ako su oba bypassana, 1.0 * 1.0 = 1.0 -> All-pass filter)
                double combinedMag = magLP * magHP;
                fftData[k] = { static_cast<float>(combinedMag), 0.0f }; // postavljamo samo realni dio, imaginarni je 0 jer želimo linearno fazni filter

                // Zrcaljenje za negativne frekvencije (druga polovica FFT buffera)
                // Ovo je nužno kako bi IFFT na izlazu dao realan impulsni odziv bez imaginarnih artefakata
                if (k > 0 && k < fftSize / 2)
                {
                    fftData[fftSize - k] = fftData[k];
                }
            }
        }

    } else { // IDEAL (Sinc ekvivalent)
        for (int k = 0; k <= fftSize / 2; ++k)
        {
            // Izračun stvarne frekvencije u Hz za trenutni FFT bin
            double f = k * sampleRate / static_cast<double>(fftSize);

            double magLP = (f <= lpCutoff || lpIsBypassed) ? 1.0 : 0.0;
            double magHP = (f >= hpCutoff || hpIsBypassed) ? 1.0 : 0.0;

            // 3. Pomnoži ih (ako su oba bypassana, 1.0 * 1.0 = 1.0 -> All-pass filter)
            fftData[k] = { static_cast<float>(magLP * magHP), 0.0f };

            // Zrcaljenje za negativne frekvencije (druga polovica FFT buffera)
                // Ovo je nužno kako bi IFFT na izlazu dao realan impulsni odziv bez imaginarnih artefakata
            if (k > 0 && k < fftSize / 2)
            {
                fftData[fftSize - k] = fftData[k];
            }
        }
    }

    // INVERZNI FFT
    fsmFFT->perform(fftData.data(), timeData.data(), true);

    int centerTap = (M - 1) / 2;

    for (int n = 0; n < M; ++n)
    {
        // Moramo prebaciti taj raspored u naš linearni M-buffer tako da vrhunac bude u centru (centerTap).
        int t = n - centerTap;

        // Čitamo iz unificiranog vremenskog odziva
        double val = (t >= 0) ? timeData[t].real() : timeData[fftSize + t].real();

        coeffs[n] = static_cast<float>(val * coeffs[n]);
    }

    juce::AudioBuffer<float> irBuffer(1, (int)M);

    irBuffer.copyFrom(0, 0, coeffs.data(), (int)M);

    conv.loadImpulseResponse(
        std::move(irBuffer),
        getSampleRate(),
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::no,
        juce::dsp::Convolution::Normalise::no
    );

    setLatencySamples(1024 + (M - 1) / 2); // Total latency is the sum of the latencies of both filters
}

juce::AudioProcessorValueTreeState::ParameterLayout FIRFilterAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hpCutoff", "High Pass Cutoff Frequency", juce::NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 10.f, juce::AudioParameterFloatAttributes()));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("lpCutoff", "Low Pass Cutoff Frequency", juce::NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 20000.f, juce::AudioParameterFloatAttributes()));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        "filterOrder",
        "Filter Order",
        juce::StringArray{ "1024", "2048", "4096", "8192", "16384" },
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
	parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
		"filterType",
		"Filter Type",
		juce::StringArray{ "Windowing Method", "FSM (Butterworth)" },
		0
	));

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
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));

			updateCoefficients(getSampleRate()); // Ažuriraj koeficijente nakon učitavanja stanja)
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FIRFilterAudioProcessor();
}
