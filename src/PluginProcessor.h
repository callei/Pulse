#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "engine/PulseEngine.h"
#include "PresetManager.h"

class PulseProcessor : public juce::AudioProcessor
{
public:
    PulseProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };
    PulseEngine engine;

    // UI state (message thread). Window width is also saved with the session.
    int uiWidth = 0;
    std::array<bool, 4> cardOpen {};
    std::atomic<bool> stateRecalled { false }; // set by setStateInformation, consumed by the editor

private:
    std::array<std::atomic<float>*, P::Count> raw {};
    uint32_t seed;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PulseProcessor)
};
