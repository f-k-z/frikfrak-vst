#pragma once
#include "dsp/Pad.h"
#include <juce_audio_basics/juce_audio_basics.h>

struct PadSound : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

// JUCE voice around dsp::Pad. Params are refreshed by the processor once per block (audio thread).
class PadVoice : public juce::SynthesiserVoice {
public:
    explicit PadVoice(const dsp::Pad::Params& p) : params(p) {}

    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<PadSound*>(s) != nullptr; }

    void setCurrentPlaybackSampleRate(double sr) override {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate(sr);
        if (sr > 0) pad.prepare(sr);
    }

    void startNote(int note, float velocity, juce::SynthesiserSound*, int) override {
        pad.noteOn((float) juce::MidiMessage::getMidiNoteInHertz(note), velocity, (uint32_t) note);
    }

    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) { pad.noteOff(); return; }
        // Hard stop (voice stealing): the steal's startNote() follows and Pad retriggers from its
        // current level and phases, so there is no click.
        // shortcut: a forced stop not followed by a steal leaves the pad state for this voice's next note; fine at pad attack times.
        clearCurrentNote();
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override {
        if (!isVoiceActive()) return;
        auto* l = out.getWritePointer(0);
        auto* r = out.getWritePointer(1);
        for (int i = start; i < start + num; ++i) {
            float a, b;
            pad.process(a, b, params);
            l[i] += a;
            r[i] += b;
        }
        if (!pad.isActive()) clearCurrentNote();
    }

private:
    dsp::Pad pad;
    const dsp::Pad::Params& params;
};
