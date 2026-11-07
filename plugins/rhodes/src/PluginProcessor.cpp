#include "PluginProcessor.h"
#include "RhodesVoice.h"

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

juce::AudioProcessorValueTreeState::ParameterLayout RhodesProcessor::createLayout() { return presets::makeLayout(rhodes::kParams); }

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

void RhodesProcessor::getStateInformation(juce::MemoryBlock& dest) { programs.save(params, dest); }
void RhodesProcessor::setStateInformation(const void* data, int size) { programs.load(params, data, size); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RhodesProcessor(); }
