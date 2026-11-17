#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

// Parameter-bound controls, all drawn natively and scaled from their own bounds.
// Every control uses juce::ParameterAttachment, so host automation, undo and gestures work.

class ParamControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParamControl(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour);
    float plain() const { return value; }
    juce::RangedAudioParameter& param;
    std::function<void(float)> onChange; // called on the message thread after every value change
protected:
    virtual void valueChanged() { repaint(); }
    juce::Colour accent;
    float value = 0.f;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Rotary knob: drag (Shift = fine), double-click = default, wheel, right-click = type a value, hover = value.
class Knob : public ParamControl
{
public:
    Knob(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour, juce::String labelOverride = {});
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
private:
    void showTextEntry();
    void setNormalised(float n, bool asGesture);
    juce::String label;
    float origin = 0.f; // normalised position the value arc starts from (0 dB / centre for bipolar)
    float lastY = 0.f;
    bool dragging = false, hover = false;
    std::unique_ptr<juce::TextEditor> editor;
};

// Pill toggle for on/off parameters. Optional activity LED (e.g. Boom trigger).
class Toggle : public ParamControl
{
public:
    Toggle(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour, juce::String text);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void flash() { ledUntil = juce::Time::getMillisecondCounter() + 120; repaint(); }
    bool showLed = false;
private:
    juce::String text;
    juce::uint32 ledUntil = 0;
};

// Power button for the module "Bypass" parameters (lit = module active, i.e. bypass off).
class PowerButton : public ParamControl
{
public:
    PowerButton(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
};

// Segmented selector for short choice lists.
class Segmented : public ParamControl
{
public:
    Segmented(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour, juce::StringArray labels = {});
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    juce::StringArray labels;
};

// Compact drop-down for longer choice lists.
class ChoiceMenu : public ParamControl
{
public:
    ChoiceMenu(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
};
