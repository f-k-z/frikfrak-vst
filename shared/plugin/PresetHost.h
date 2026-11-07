#pragma once
// JUCE glue for presets::ParamSpec / presets::Preset tables: parameter layout, host programs,
// current program saved in the plugin state.
#include "presets/PresetTable.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

namespace presets {

template <std::size_t N>
juce::AudioProcessorValueTreeState::ParameterLayout makeLayout(const std::array<ParamSpec, N>& specs) {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (const auto& p : specs)
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { p.id, 1 }, p.name, juce::NormalisableRange<float>(p.min, p.max, 0.0f, p.skew), p.def));
    return l;
}

template <std::size_t N, std::size_t P>
class Programs {
public:
    Programs(const std::array<ParamSpec, N>& s, const std::array<Preset<N>, P>& t) : specs(s), table(t) {}

    int count() const { return (int) P; }
    int current() const { return program; }
    juce::String name(int i) const { return juce::isPositiveAndBelow(i, (int) P) ? juce::String::fromUTF8(table[(size_t) i].name) : juce::String(); }

    void apply(juce::AudioProcessorValueTreeState& apvts, int i) {
        if (!juce::isPositiveAndBelow(i, (int) P)) return;
        for (size_t k = 0; k < N; ++k)
            if (auto* p = apvts.getParameter(specs[k].id)) p->setValueNotifyingHost(p->convertTo0to1(table[(size_t) i].v[k]));
        program = i;
    }

    void save(juce::AudioProcessorValueTreeState& apvts, juce::MemoryBlock& dest) const {
        auto state = apvts.copyState();
        state.setProperty("program", program.load(), nullptr);
        if (auto xml = state.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, dest);
    }

    void load(juce::AudioProcessorValueTreeState& apvts, const void* data, int size) {
        if (auto xml = juce::AudioProcessor::getXmlFromBinary(data, size); xml && xml->hasTagName(apvts.state.getType())) {
            const auto state = juce::ValueTree::fromXml(*xml);
            program = (int) state.getProperty("program", 0);
            apvts.replaceState(state);
        }
    }

private:
    const std::array<ParamSpec, N>& specs;
    const std::array<Preset<N>, P>& table;
    std::atomic<int> program { 0 };
};

}  // namespace presets
