#include "PluginProcessor.h"

namespace {
juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    auto pct = [](const char* id, const char* name, float def) {
        return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float>(0.0f, 1.0f), def);
    };
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(pct("crackle", "Crackle", 0.3f));
    l.add(pct("hiss", "Hiss", 0.3f));
    l.add(pct("age", "Age", 0.4f));
    return l;
}
}  // namespace

VinylNoiseProcessor::VinylNoiseProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "params", createLayout()) {}

bool VinylNoiseProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VinylNoiseProcessor::prepareToPlay(double sampleRate, int) {
    vinyl.prepare(sampleRate);
    held.fill(false);
    heldNotes = 0;
}

void VinylNoiseProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();  // instrument: no input, the noise is the whole sound
    dsp::Vinyl::Params vp { get("crackle"), get("hiss"), 0.0f, get("age"), true };
    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getWritePointer(1);
    int i = 0;
    auto renderTo = [&](int end) {
        vp.gate = heldNotes > 0;
        vp.pitch = std::exp2((float) (note - 60) / 12.0f);  // C4 = reference colour, 1 octave = x2
        vp.level = velocity;
        for (; i < end; ++i) vinyl.process(l[i], r[i], vp);
    };
    for (const auto meta : midi) {  // sample-accurate gate
        renderTo(meta.samplePosition);
        const auto m = meta.getMessage();
        auto& h = held[(size_t) m.getNoteNumber()];
        if (m.isNoteOn()) {
            if (!h) { h = true; ++heldNotes; }
            note = m.getNoteNumber();  // last note wins
            velocity = m.getFloatVelocity();
        } else if (m.isNoteOff()) {
            if (h) { h = false; --heldNotes; }
            if (m.getNoteNumber() == note)  // fall back to the highest key still held
                for (int k = 127; k >= 0; --k) if (held[(size_t) k]) { note = k; break; }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { held.fill(false); heldNotes = 0; }
    }
    renderTo(buffer.getNumSamples());
}

void VinylNoiseProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = params.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void VinylNoiseProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(params.state.getType()))
        params.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VinylNoiseProcessor(); }
