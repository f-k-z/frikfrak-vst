#include "PluginProcessor.h"
#include "PadVoice.h"

namespace {
constexpr int kVoices = 24;  // long releases + sustain pedal: keep hard voice-steals rare
}

KankyoProcessor::KankyoProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "params", presets::makeLayout(kankyo::kParams)) {
    for (int i = 0; i < kVoices; ++i) synth.addVoice(new PadVoice(padParams));
    synth.addSound(new PadSound());
}

bool KankyoProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void KankyoProcessor::prepareToPlay(double sampleRate, int) {
    synth.setCurrentPlaybackSampleRate(sampleRate);
    chorus.prepare(sampleRate, get("chorus"));
    reverb.prepare(sampleRate, get("space"), get("size"));
    volumeS.prepare(sampleRate, juce::Decibels::decibelsToGain(get("volume")));
}

void KankyoProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    padParams = { get("blend"), get("shape"), get("detune"), get("color"), get("ratio"), get("bright"), get("air"), get("motion"),
                  get("attack"), get("decay"), get("sustain"), get("release") };
    buffer.clear();
    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());

    const float ch = get("chorus"), space = get("space"), size = get("size"), volume = juce::Decibels::decibelsToGain(get("volume"));
    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        float a = l[i], b = r[i];
        chorus.process(a, b, ch);
        reverb.process(a, b, space, size);
        const float g = volumeS.next(volume) * kankyo::kOutputMakeup;
        l[i] = juce::jlimit(-1.0f, 1.0f, a * g);  // hard safety ceiling
        r[i] = juce::jlimit(-1.0f, 1.0f, b * g);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KankyoProcessor(); }
