#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"

// Factory + user presets, A/B compare. Message thread only.
// Presets store every parameter except Quality and Match (those are workflow settings, not sound).
// User presets are XML files in Documents/Pulse/Presets.
class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& state);

    struct Factory { const char* name; std::vector<std::pair<int, float>> values; };
    static const std::vector<Factory>& factory();
    static juce::File userFolder();

    juce::StringArray userNames() const;
    int numPresets() const { return (int) factory().size() + userNames().size(); }
    bool load(int index);              // factory first, then user presets (alphabetical); false if unreadable
    void step(int delta);              // previous / next
    bool saveUser(const juce::String& name);
    juce::String currentName() const;
    bool isModified() const;           // current sound differs from the loaded preset
    void markLoaded();                 // treat the current sound as "unmodified" (after a host state recall)

    // A/B: two independent sounds. Switching stores the current sound in the active slot.
    int activeSlot() const { return slot; }
    void selectSlot(int s);
    void copyActiveToOther();

private:
    P::Values capture() const;
    void apply(const P::Values& v, const juce::String& name);
    static bool isPresetParam(int id) { return id != P::Quality && id != P::AutoGain; }

    juce::AudioProcessorValueTreeState& apvts;
    P::Values loaded;
    int slot = 0;
    P::Values slots[2];
    juce::String slotNames[2];
    bool slotValid[2] { false, false };
    int currentIndex = 0;
};
