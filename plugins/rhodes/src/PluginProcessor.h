#pragma once
#include "Presets.h"
#include "dsp/Effects.h"
#include "plugin/PresetHost.h"
#include "ui/FrikfrakEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>

class RhodesProcessor : public juce::AudioProcessor {
public:
    RhodesProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return new ui::Editor(*this, params, juce::String::fromUTF8("Electric piano · Mark I")); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return programs.count(); }
    int getCurrentProgram() override { return programs.current(); }
    void setCurrentProgram(int i) override { programs.apply(params, i); }  // factory preset (Presets.h)
    const juce::String getProgramName(int i) override { return programs.name(i); }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState params;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float get(const char* id) const { return params.getRawParameterValue(id)->load(); }

    juce::Synthesiser synth;
    dsp::Smoother volumeS, driveS, depthS;
    dsp::Tremolo tremolo;
    dsp::Tone tone;
    dsp::Phaser phaser;
    presets::Programs<rhodes::kParams.size(), rhodes::kPresets.size()> programs { rhodes::kParams, rhodes::kPresets };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhodesProcessor)
};
