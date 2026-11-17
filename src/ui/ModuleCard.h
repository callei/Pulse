#pragma once
#include "Controls.h"
#include <map>

// One module card: power button, 1-2 hero knobs, main selectors, and an expandable detail area.
// Built from a spec so all four modules share the same layout rules.
struct CardSpec
{
    struct Hero { int id; const char* label; int dependsOn; }; // dependsOn: toggle id that greys it out, or -1
    struct Section { const char* title; int toggleId; std::vector<int> items; };
    int module;
    const char* title;
    int bypassId;
    std::vector<Hero> heroes;
    std::vector<int> compact;   // always visible (choices/toggles); toggles reused as section headers move there when open
    std::vector<Section> detail;
};

class ModuleCard : public juce::Component
{
public:
    ModuleCard(juce::AudioProcessorValueTreeState& s, CardSpec spec);
    void paint(juce::Graphics&) override;
    void resized() override;

    void setExpanded(bool e);
    bool isExpanded() const { return expanded; }
    std::function<void(bool)> onExpandChanged;
    ParamControl* control(int id) { auto it = ctrls.find(id); return it == ctrls.end() ? nullptr : it->second.get(); }
    void setExtra(juce::Component* c) { extra = c; if (c) content.addAndMakeVisible(c); resized(); }

private:
    ParamControl& make(int id, const char* labelOverride = nullptr);
    void updateDimming();
    bool isSectionToggle(int id) const;

    juce::AudioProcessorValueTreeState& apvts;
    CardSpec spec;
    juce::Colour accent;
    std::unique_ptr<PowerButton> power;
    juce::TextButton moreButton;
    juce::Component content;        // everything except the header; faded when the module is off
    std::map<int, std::unique_ptr<ParamControl>> ctrls;
    std::map<int, int> dependsOn;   // control id -> toggle id
    juce::Component* extra = nullptr;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> titles; // section titles (card coordinates)
    bool expanded = false;
    float headerH = 30.f;
};
