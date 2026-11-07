#include "PluginProcessor.h"
#include "GlitchEditor.h"

namespace {
juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (const auto& k : glitch::kKnobs) {
        if (k.divs) {
            juce::StringArray labels;
            for (int i = 0; i < k.nDivs; ++i) labels.add(k.divs[i].label);
            l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { k.id, 1 }, k.name, labels, (int) k.def));
        } else {
            l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { k.id, 1 }, k.name, juce::NormalisableRange<float>(k.min, k.max, k.step), k.def));
        }
    }
    // Pads after the knobs: Live's control surfaces put the controller's knobs on the first 8 parameters.
    for (const auto& p : glitch::kPads)
        l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { p.id, 1 }, p.name, false));
    return l;
}
}  // namespace

GlitchProcessor::GlitchProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "params", createLayout()) {
    for (size_t k = 0; k < glitch::kKnobs.size(); ++k) knobParams[k] = params.getParameter(glitch::kKnobs[k].id);
}

bool GlitchProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorEditor* GlitchProcessor::createEditor() { return new glitch::GlitchEditor(*this); }

void GlitchProcessor::prepareToPlay(double sampleRate, int) {
    setLatencySamples(0);  // on the Main track any declared latency would delay everything, pads off or not
    clock.prepare(sampleRate);
    chain.prepare(sampleRate);
    pads.reset();
}

void GlitchProcessor::readSettings() {
    auto choice = [this](const char* id, const glitch::Division* divs, int n) {
        return divs[juce::jlimit(0, n - 1, (int) get(id))].qn;
    };
    auto& s = settings;
    s.crushBits = get("crush_bits");
    s.crushRateKHz = get("crush_rate");
    s.pitchSemis = get("pitch_semi");
    s.reverbSize = get("reverb_size");
    s.stutterQn = choice("stutter_div", glitch::kStutterDivs.data(), (int) glitch::kStutterDivs.size());
    s.gateQn = choice("gate_rate", glitch::kGateDivs.data(), (int) glitch::kGateDivs.size());
    s.gateDepth = get("gate_depth");
    s.phaserHz = get("phaser_speed");
    s.filterFreq = get("filter_freq");
    s.tapeQn = choice("tape_time", glitch::kTapeDivs.data(), (int) glitch::kTapeDivs.size());
    s.reverseQn = choice("reverse_len", glitch::kReverseDivs.data(), (int) glitch::kReverseDivs.size());
    s.echoQn = choice("echo_time", glitch::kEchoDivs.data(), (int) glitch::kEchoDivs.size());
    s.echoFeedback = get("echo_feedback");
    s.downerQn = choice("downer_rate", glitch::kDownerDivs.data(), (int) glitch::kDownerDivs.size());
    s.downerDepth = get("downer_depth");
}

void GlitchProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    bool hasBpm = false, hasPpq = false, playing = false;
    double bpm = 0, ppq = 0;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition()) {
            if (const auto b = pos->getBpm()) { hasBpm = true; bpm = *b; }
            if (const auto q = pos->getPpqPosition()) { hasPpq = true; ppq = *q; }
            playing = pos->getIsPlaying();
        }
    clock.begin(n, hasBpm, bpm, hasPpq, ppq, playing);
    readSettings();

    for (int i = 0; i < (int) glitch::kPads.size(); ++i) pads.setParam(i, get(glitch::kPads[(size_t) i].id) > 0.5f);
    pads.update();

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getWritePointer(1);
    dsp::GlitchChain::Active active {};
    int i = 0;
    auto renderTo = [&](int end) {
        for (int k = 0; k < dsp::GlitchChain::kFx; ++k) active[(size_t) k] = pads.active(k);
        for (; i < end; ++i) chain.process(l[i], r[i], active, settings, clock, clock.ppqAt(i));
    };
    for (const auto meta : midi) {  // sample-accurate pads
        renderTo(std::min(meta.samplePosition, n));
        const auto m = meta.getMessage();
        if (m.isNoteOn()) pads.noteOn(m.getNoteNumber());
        else if (m.isNoteOff()) pads.noteOff(m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) pads.allNotesOff();
        else if (m.isController())  // hardware knob: moves the parameter so the UI and the host follow
            if (const int k = glitch::knobForCC(m.getControllerNumber(), pads.lastPressed()); k >= 0)
                knobParams[(size_t) k]->setValueNotifyingHost((float) m.getControllerValue() / 127.0f);
        pads.update();
    }
    renderTo(n);
    activeMask.store(pads.mask(), std::memory_order_relaxed);
}

void GlitchProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = params.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void GlitchProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(params.state.getType()))
        params.replaceState(juce::ValueTree::fromXml(*xml));
    // Pads never come back engaged with a set: only the knobs are restored.
    for (const auto& p : glitch::kPads)
        if (auto* prm = params.getParameter(p.id)) prm->setValueNotifyingHost(0.0f);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GlitchProcessor(); }
