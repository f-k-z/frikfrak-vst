#pragma once
#include "dsp/Effects.h"
#include <juce_audio_utils/juce_audio_utils.h>

class RhodesProcessor : public juce::AudioProcessor {
public:
    RhodesProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhodesProcessor)
};
