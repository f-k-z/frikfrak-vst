#pragma once
#include "dsp/RhodesTine.h"
#include <juce_audio_basics/juce_audio_basics.h>

struct RhodesSound : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class RhodesVoice : public juce::SynthesiserVoice {
public:
    RhodesVoice(std::atomic<float>* barkParam, std::atomic<float>* decayParam) : bark(barkParam), decay(decayParam) {}

    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<RhodesSound*>(s) != nullptr; }

    void setCurrentPlaybackSampleRate(double sr) override {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate(sr);
        if (sr > 0) tine.prepare(sr);
    }

    void startNote(int note, float velocity, juce::SynthesiserSound*, int) override {
        tine.noteOn(note, velocity, bark->load(), decay->load());
    }

    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) { tine.noteOff(); return; }
        // Hard stop (JUCE voice stealing). The steal's startNote() follows immediately and
        // RhodesTine crossfades from the last sample, so we just free the voice.
        // shortcut: a host-forced hard stop not followed by a steal leaves a stale residual
        // that is replayed as a ~3 ms fade on this voice's next note; upgrade if it becomes audible.
        clearCurrentNote();
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override {
        if (!isVoiceActive()) return;
        auto* l = out.getWritePointer(0);
        auto* r = out.getNumChannels() > 1 ? out.getWritePointer(1) : nullptr;
        for (int i = start; i < start + num; ++i) {
            const float s = tine.process();
            l[i] += s;
            if (r) r[i] += s;
        }
        if (!tine.isActive()) clearCurrentNote();
    }

private:
    dsp::RhodesTine tine;
    std::atomic<float>* bark;
    std::atomic<float>* decay;
};
