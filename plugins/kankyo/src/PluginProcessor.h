#pragma once
#include "Presets.h"
#include "dsp/Pad.h"
#include "dsp/Space.h"
#include "plugin/PresetHost.h"
#include "ui/FrikfrakEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>

class KankyoProcessor : public juce::AudioProcessor {
public:
    KankyoProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return new ui::Editor(*this, params, juce::String::fromUTF8("Kankyō ongaku")); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 22.0; }  // max release + max reverb decay

    int getNumPrograms() override { return programs.count(); }
    int getCurrentProgram() override { return programs.current(); }
    void setCurrentProgram(int i) override { programs.apply(params, i); }
    const juce::String getProgramName(int i) override { return programs.name(i); }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override { programs.save(params, dest); }
    void setStateInformation(const void* data, int size) override { programs.load(params, data, size); }

    juce::AudioProcessorValueTreeState params;

private:
    float get(const char* id) const { return params.getRawParameterValue(id)->load(); }

    juce::Synthesiser synth;
    dsp::Pad::Params padParams;
    dsp::Chorus chorus;
    dsp::Reverb reverb;
    dsp::Smoother volumeS;
    presets::Programs<kankyo::kParams.size(), kankyo::kPresets.size()> programs { kankyo::kParams, kankyo::kPresets };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KankyoProcessor)
};
