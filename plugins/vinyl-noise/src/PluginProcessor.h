#pragma once
#include "dsp/Vinyl.h"
#include <juce_audio_utils/juce_audio_utils.h>

class VinylNoiseProcessor : public juce::AudioProcessor {
public:
    VinylNoiseProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // The record keeps spinning (crackle/hiss) on silent input.
    double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); }

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VinylNoiseProcessor)
};
