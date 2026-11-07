#pragma once
#include "dsp/Vinyl.h"
#include "ui/FrikfrakEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>

class VinylNoiseProcessor : public juce::AudioProcessor {
public:
    VinylNoiseProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return new ui::Editor(*this, params, "Keys drop the needle"); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }  // MIDI keys gate crackle/hiss
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }  // gate release + crackle ring-out

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState params;

private:
    float get(const char* id) const { return params.getRawParameterValue(id)->load(); }

    dsp::Vinyl vinyl;
    std::array<bool, 128> held {};
    int heldNotes = 0, note = 60;
    float velocity = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VinylNoiseProcessor)
};
