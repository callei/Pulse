#pragma once
#include "PluginProcessor.h"
#include "ui/ModuleCard.h"
#include "ui/Displays.h"
#include "ui/PulseLookAndFeel.h"

// One page: header (presets, A/B, undo, quality) / IN column / TAPE GRIT SHAPE PUNCH / OUT column.
class PulseEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit PulseEditor(PulseProcessor&);
    ~PulseEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = theme::baseW, kBaseH = theme::baseH;

private:
    void timerCallback() override;
    void showPresetMenu();
    void savePresetDialog();

    PulseProcessor& proc;
    PulseLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<ModuleCard> cards[4];
    GainTrace trace { theme::accent(theme::Shape) };
    std::unique_ptr<Knob> inputKnob, outputKnob, mixKnob, widthKnob;
    std::unique_ptr<Segmented> quality;
    std::unique_ptr<Toggle> match;
    LevelMeter inMeter, outMeter;

    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, presetName, savePreset { "Save" };
    juce::TextButton slotA { "A" }, slotB { "B" }, copyAB { "Copy" }, undoButton { "Undo" }, redoButton { "Redo" };

    int lastBoomCount = 0;
    float matchDb = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PulseEditor)
};
