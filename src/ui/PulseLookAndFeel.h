#pragma once
#include "Theme.h"

// Native JUCE components (buttons, menus, dialogs, tooltips) in the Pulse palette.
class PulseLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PulseLookAndFeel()
    {
        setColourScheme({ theme::panel, theme::bg, theme::panelHi, theme::border, theme::text, theme::accent(2),
                          theme::text, theme::panelHi, theme::text });
        setColour(juce::TextButton::buttonColourId, theme::panelHi);
        setColour(juce::TextButton::buttonOnColourId, theme::neutral.withAlpha(0.25f));
        setColour(juce::TextButton::textColourOffId, theme::dim);
        setColour(juce::TextButton::textColourOnId, theme::text);
        setColour(juce::ComboBox::outlineColourId, theme::border);
        setColour(juce::PopupMenu::backgroundColourId, theme::panelHi);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::track);
        setColour(juce::PopupMenu::textColourId, theme::text);
        setColour(juce::PopupMenu::headerTextColourId, theme::dim);
        setColour(juce::TooltipWindow::backgroundColourId, theme::panelHi);
        setColour(juce::TooltipWindow::textColourId, theme::text);
        setColour(juce::TooltipWindow::outlineColourId, theme::border);
        setColour(juce::AlertWindow::backgroundColourId, theme::panel);
        setColour(juce::AlertWindow::textColourId, theme::text);
        setColour(juce::AlertWindow::outlineColourId, theme::border);
        setColour(juce::TextEditor::backgroundColourId, theme::bg);
        setColour(juce::TextEditor::outlineColourId, theme::border);
        setColour(juce::TextEditor::focusedOutlineColourId, theme::neutral);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        const float rad = r.getHeight() * 0.25f;
        auto c = b.getToggleState() ? b.findColour(juce::TextButton::buttonOnColourId) : bg;
        if (over || down) c = c.brighter(down ? 0.15f : 0.07f);
        g.setColour(c);
        g.fillRoundedRectangle(r, rad);
        g.setColour(b.getToggleState() ? theme::neutral : theme::border);
        g.drawRoundedRectangle(r, rad, 1.f);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override { return theme::font(std::max(9.f, buttonHeight * 0.5f)); }
    juce::Font getPopupMenuFont() override { return theme::font(14.f); }
};
