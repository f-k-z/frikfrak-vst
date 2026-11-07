#include "PluginProcessor.h"
#include "RhodesVoice.h"
#include "Presets.h"

namespace {
constexpr int kVoices = 32;  // generous so JUCE's hard voice-steal stays rare with the pedal down
}

RhodesProcessor::RhodesProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "params", createLayout()) {
    for (int i = 0; i < kVoices; ++i)
        synth.addVoice(new RhodesVoice(params.getRawParameterValue("bark"), params.getRawParameterValue("decay")));
    synth.addSound(new RhodesSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout RhodesProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (const auto& p : rhodes::kParams)
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { p.id, 1 }, p.name, juce::NormalisableRange<float>(p.min, p.max), p.def));
    return l;
}

int RhodesProcessor::getNumPrograms() { return (int) rhodes::kPresets.size(); }

const juce::String RhodesProcessor::getProgramName(int i) {
    return juce::isPositiveAndBelow(i, (int) rhodes::kPresets.size()) ? juce::String::fromUTF8(rhodes::kPresets[(size_t) i].name) : juce::String();
}

void RhodesProcessor::setCurrentProgram(int i) {
    if (!juce::isPositiveAndBelow(i, (int) rhodes::kPresets.size())) return;
    const auto& preset = rhodes::kPresets[(size_t) i];
    for (size_t k = 0; k < rhodes::kParams.size(); ++k)
        if (auto* p = params.getParameter(rhodes::kParams[k].id))
            p->setValueNotifyingHost(p->convertTo0to1(preset.v[k]));
    program = i;
}

bool RhodesProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RhodesProcessor::prepareToPlay(double sampleRate, int) {
    synth.setCurrentPlaybackSampleRate(sampleRate);
    volumeS.prepare(sampleRate, juce::Decibels::decibelsToGain(get("volume")));
    driveS.prepare(sampleRate, get("drive"));
    depthS.prepare(sampleRate, get("tremDepth"));
    tremolo.prepare(sampleRate);
    tone.prepare(sampleRate, get("tone"));
    phaser.prepare(sampleRate, get("phaser"));
}

void RhodesProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());

    const float volume = juce::Decibels::decibelsToGain(get("volume")), drive = get("drive");
    const float rate = get("tremRate"), depth = get("tremDepth"), toneV = get("tone"), phaserV = get("phaser");

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float d = driveS.next(drive), g = volumeS.next(volume);
        float a = dsp::drive(l[i], d) * g, b = dsp::drive(r[i], d) * g;
        tone.process(a, b, toneV);
        phaser.process(a, b, phaserV);
        tremolo.process(a, b, rate, depthS.next(depth));
        l[i] = juce::jlimit(-1.0f, 1.0f, a);  // hard safety ceiling, never reached at sane settings
        r[i] = juce::jlimit(-1.0f, 1.0f, b);
    }
}

void RhodesProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = params.copyState();
    state.setProperty("program", program.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void RhodesProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(params.state.getType())) {
        const auto state = juce::ValueTree::fromXml(*xml);
        program = (int) state.getProperty("program", 0);
        params.replaceState(state);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RhodesProcessor(); }
