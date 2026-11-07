#pragma once
#include "Params.h"
#include "dsp/Chain.h"
#include "dsp/Pads.h"
#include "ui/FrikfrakEditor.h"
#include <atomic>
#include <juce_audio_utils/juce_audio_utils.h>

class GlitchProcessor : public juce::AudioProcessor {
public:
    GlitchProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }  // notes 36..47 play the pads
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }  // max echo ring-out / reverb decay

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState params;
    std::atomic<uint32_t> activeMask { 0 };  // bit i = effect i active, bit 11 = Hold (read by the editor)

private:
    float get(const char* id) const { return params.getRawParameterValue(id)->load(); }
    void readSettings();

    dsp::Pads pads;
    dsp::BeatClock clock;
    dsp::GlitchChain chain;
    dsp::GlitchSettings settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchProcessor)
};
