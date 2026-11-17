#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Dark, calm, high-contrast palette. One restrained accent per module.
namespace theme
{
inline const juce::Colour bg { 0xff0f1114 };
inline const juce::Colour panel { 0xff171a1e };
inline const juce::Colour panelHi { 0xff1d2126 };
inline const juce::Colour border { 0xff282d34 };
inline const juce::Colour track { 0xff2c3138 };
inline const juce::Colour text { 0xffe9ebee };
inline const juce::Colour dim { 0xff8d949e };
inline const juce::Colour faint { 0xff4b525c };
inline const juce::Colour neutral { 0xffc7ccd4 }; // global controls

inline constexpr int baseW = 1120, baseH = 640; // editor design size; everything scales from the editor height
// UI scale of the editor window that contains c (each plugin instance has its own size).
inline float scaleOf(const juce::Component& c)
{
    for (auto* p = c.getParentComponent(); p != nullptr; p = p->getParentComponent())
        if (dynamic_cast<juce::AudioProcessorEditor*>(p) != nullptr) return (float) p->getHeight() / (float) baseH;
    return 1.f;
}

enum Module { Tape = 0, Grit, Shape, Punch };
inline juce::Colour accent(int m)
{
    static const juce::Colour c[] = { juce::Colour(0xffe3a752), juce::Colour(0xffe8735f), juce::Colour(0xff4fc6b9), juce::Colour(0xffa193ff) };
    return c[juce::jlimit(0, 3, m)];
}
inline juce::Font font(float height, bool bold = false)
{
    return juce::Font(juce::FontOptions(height).withStyle(bold ? "Bold" : "Regular"));
}
} // namespace theme
