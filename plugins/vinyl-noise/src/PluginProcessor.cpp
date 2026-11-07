#include "PluginProcessor.h"

namespace {
juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    auto pct = [](const char* id, const char* name, float def) {
        return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float>(0.0f, 1.0f), def);
    };
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(pct("crackle", "Crackle", 0.3f));
    l.add(pct("hiss", "Hiss", 0.3f));
    l.add(pct("wow", "Wow / Flutter", 0.3f));
    l.add(pct("age", "Age", 0.4f));
    return l;
}
}  // namespace

VinylNoiseProcessor::VinylNoiseProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "params", createLayout()) {}

bool VinylNoiseProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void VinylNoiseProcessor::prepareToPlay(double sampleRate, int) {
    vinyl.prepare(sampleRate);
    setLatencySamples(vinyl.latencySamples());
}

void VinylNoiseProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const dsp::Vinyl::Params vp { get("crackle"), get("hiss"), get("wow"), get("age"), true };
    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        vinyl.process(l[i], r[i], vp);
}

void VinylNoiseProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = params.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void VinylNoiseProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(params.state.getType()))
        params.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VinylNoiseProcessor(); }
