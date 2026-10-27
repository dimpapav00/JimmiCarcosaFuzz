#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout CarcosaAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto knob = [&p] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                  juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), def));
    };
    knob ("before", "Before",  0.65f);
    knob ("after",  "After",   0.30f);
    knob ("output", "Output",  0.50f);
    knob ("hicut",  "Hi-Cut",  0.30f);
    knob ("octave", "Octave",  0.00f);
    knob ("bass",   "Bass",    0.50f);
    knob ("mid",    "Mid",     0.50f);
    knob ("treble", "Treble",  0.50f);

    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode",
                                                               juce::StringArray { "Demhe", "Hali" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "octmode", 1 }, "Octave Mode",
                                                               juce::StringArray { "Down", "Up" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "input", 1 }, "Input",
                                                              juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "trim", 1 }, "Trim",
                                                              juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gate", 1 }, "Noise Gate",
                                                              juce::NormalisableRange<float> (-80.0f, -30.0f, 0.1f), -60.0f));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "On", true));
    return { p.begin(), p.end() };
}

CarcosaAudioProcessor::CarcosaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pBefore = apvts.getRawParameterValue ("before");
    pAfter = apvts.getRawParameterValue ("after");
    pOutput = apvts.getRawParameterValue ("output");
    pHiCut = apvts.getRawParameterValue ("hicut");
    pIn = apvts.getRawParameterValue ("input");
    pTrim = apvts.getRawParameterValue ("trim");
    pGate = apvts.getRawParameterValue ("gate");
    pOn = apvts.getRawParameterValue ("on");
    pMode = apvts.getRawParameterValue ("mode");
    pOct = apvts.getRawParameterValue ("octave");
    pOctMode = apvts.getRawParameterValue ("octmode");
    pBass = apvts.getRawParameterValue ("bass");
    pMid = apvts.getRawParameterValue ("mid");
    pTreble = apvts.getRawParameterValue ("treble");
}

void CarcosaAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

bool CarcosaAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void CarcosaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    carcosa::Params p;
    p.before  = pBefore->load();
    p.after   = pAfter->load();
    p.output  = pOutput->load();
    p.hiCut   = pHiCut->load();
    p.inputDb = pIn->load();
    p.trimDb  = pTrim->load();
    p.gateDb  = pGate->load();
    p.on      = pOn->load() > 0.5f;
    p.demhe   = (int) std::lround (pMode->load()) == 0;
    p.octave  = pOct->load();
    p.octUp   = (int) std::lround (pOctMode->load()) == 1;
    p.bass    = pBass->load();
    p.mid     = pMid->load();
    p.treble  = pTreble->load();

    engine.process (buffer, p);
}

juce::AudioProcessorEditor* CarcosaAudioProcessor::createEditor() { return new CarcosaEditor (*this); }

void CarcosaAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void CarcosaAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CarcosaAudioProcessor(); }
