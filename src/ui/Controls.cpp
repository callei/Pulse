#include "Controls.h"
#include "../Params.h"

namespace
{
int idOf(const juce::RangedAudioParameter& p) { return P::findByIdString(p.getParameterID()); }
juce::String tooltipFor(const juce::RangedAudioParameter& p)
{
    const int id = idOf(p);
    return id >= 0 ? juce::String(P::def(id).tooltip) : juce::String();
}
} // namespace

// ---- ParamControl -------------------------------------------------------------------------------
ParamControl::ParamControl(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour)
    : param(*s.getParameter(paramId)), accent(accentColour)
{
    attachment = std::make_unique<juce::ParameterAttachment>(param, [this](float v) { value = v; valueChanged(); if (onChange) onChange(v); }, s.undoManager);
    attachment->sendInitialUpdate();
    setTooltip(tooltipFor(param));
}

// ---- Knob ---------------------------------------------------------------------------------------
Knob::Knob(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour, juce::String labelOverride)
    : ParamControl(s, paramId, accentColour)
{
    const int id = idOf(param);
    label = labelOverride.isNotEmpty() ? labelOverride : (id >= 0 ? juce::String(P::def(id).label) : param.getName(20));
    const auto range = param.getNormalisableRange();
    origin = (range.start < 0.f && range.end > 0.f) ? range.convertTo0to1(0.f) : 0.f;
    setRepaintsOnMouseActivity(true);
}

void Knob::resized()
{
    if (editor) editor->setBounds(getLocalBounds().removeFromBottom(juce::roundToInt(getHeight() * 0.24f)));
}

void Knob::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float ui = theme::scaleOf(*this);
    const bool hero = b.getHeight() > 100.f * ui;
    const float fontH = (hero ? 14.f : 11.5f) * ui;
    auto textArea = b.removeFromBottom(fontH * 1.6f);
    const float d = std::min(b.getWidth(), b.getHeight()) * 0.92f;
    auto knob = juce::Rectangle<float>(d, d).withCentre(b.getCentre());
    const float r = d * 0.5f, cx = knob.getCentreX(), cy = knob.getCentreY();
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float norm = param.convertTo0to1(value);
    const float stroke = std::max(2.f, d * 0.075f);

    juce::Path bgArc, valArc;
    bgArc.addCentredArc(cx, cy, r - stroke, r - stroke, 0.f, a0, a1, true);
    g.setColour(theme::track);
    g.strokePath(bgArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float aOrigin = a0 + (a1 - a0) * origin, aVal = a0 + (a1 - a0) * norm;
    if (std::abs(aVal - aOrigin) > 0.001f)
    {
        valArc.addCentredArc(cx, cy, r - stroke, r - stroke, 0.f, std::min(aOrigin, aVal), std::max(aOrigin, aVal), true);
        g.setColour(isEnabled() ? accent : theme::faint);
        g.strokePath(valArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // body + pointer
    auto body = knob.reduced(stroke * 2.2f);
    g.setColour(hover || dragging ? theme::panelHi.brighter(0.08f) : theme::panelHi);
    g.fillEllipse(body);
    g.setColour(theme::border);
    g.drawEllipse(body, 1.f);
    const float pr = body.getWidth() * 0.5f;
    juce::Point<float> p1(cx + std::sin(aVal) * pr * 0.35f, cy - std::cos(aVal) * pr * 0.35f), p2(cx + std::sin(aVal) * pr * 0.85f, cy - std::cos(aVal) * pr * 0.85f);
    g.setColour(theme::text);
    g.drawLine({ p1, p2 }, std::max(1.5f, d * 0.04f));

    // label, or the value while hovering/dragging
    const bool showValue = hover || dragging;
    g.setColour(showValue ? theme::text : theme::dim);
    g.setFont(theme::font(fontH, showValue));
    g.drawFittedText(showValue ? param.getCurrentValueAsText() : label, textArea.toNearestInt(), juce::Justification::centred, 1);
}

void Knob::setNormalised(float n, bool asGesture)
{
    const float v = param.convertFrom0to1(juce::jlimit(0.f, 1.f, n));
    if (asGesture) attachment->setValueAsPartOfGesture(v);
    else attachment->setValueAsCompleteGesture(v);
}

void Knob::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showTextEntry(); return; }
    dragging = true;
    lastY = e.position.y;
    attachment->beginGesture();
    repaint();
}

void Knob::mouseDrag(const juce::MouseEvent& e)
{
    if (! dragging) return;
    const float dy = lastY - e.position.y;
    lastY = e.position.y;
    const float pixelsForFullRange = (e.mods.isShiftDown() ? 1500.f : 200.f) * std::max(0.6f, getHeight() / 80.f);
    setNormalised(param.convertTo0to1(value) + dy / pixelsForFullRange, true);
}

void Knob::mouseUp(const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    attachment->endGesture();
    repaint();
}

void Knob::mouseDoubleClick(const juce::MouseEvent&)
{
    attachment->setValueAsCompleteGesture(param.convertFrom0to1(param.getDefaultValue()));
}

void Knob::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float step = (e.mods.isShiftDown() ? 0.005f : 0.03f) * (w.deltaY > 0 ? 1.f : -1.f) * (w.isReversed ? -1.f : 1.f);
    if (w.deltaY != 0.f) setNormalised(param.convertTo0to1(value) + step, false);
}

void Knob::showTextEntry()
{
    editor = std::make_unique<juce::TextEditor>();
    editor->setJustification(juce::Justification::centred);
    editor->setColour(juce::TextEditor::backgroundColourId, theme::bg);
    editor->setColour(juce::TextEditor::textColourId, theme::text);
    editor->setColour(juce::TextEditor::outlineColourId, accent);
    editor->setColour(juce::TextEditor::focusedOutlineColourId, accent);
    editor->setFont(theme::font(getHeight() * 0.17f));
    editor->setText(param.getCurrentValueAsText(), false);
    editor->selectAll();
    auto commit = [this, ed = editor.get()] {
        if (editor.get() != ed || ! ed->isVisible()) return;
        ed->setVisible(false); // marks it done: a later focus-lost won't commit again
        const auto t = ed->getText();
        if (t.trim().isNotEmpty()) attachment->setValueAsCompleteGesture(param.convertFrom0to1(param.getValueForText(t)));
        juce::MessageManager::callAsync([sp = juce::Component::SafePointer<Knob>(this), ed] { if (sp && sp->editor.get() == ed) sp->editor.reset(); });
    };
    editor->onReturnKey = commit;
    editor->onFocusLost = commit;
    editor->onEscapeKey = [this, ed = editor.get()] {
        ed->setVisible(false);
        juce::MessageManager::callAsync([sp = juce::Component::SafePointer<Knob>(this), ed] { if (sp && sp->editor.get() == ed) sp->editor.reset(); });
    };
    addAndMakeVisible(*editor);
    resized();
    editor->grabKeyboardFocus();
}

// ---- Toggle -------------------------------------------------------------------------------------
Toggle::Toggle(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour, juce::String t)
    : ParamControl(s, paramId, accentColour), text(std::move(t)) {}

void Toggle::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.f);
    const bool on = value >= 0.5f;
    const float rad = b.getHeight() * 0.5f;
    g.setColour(on ? accent.withAlpha(0.18f) : theme::panelHi);
    g.fillRoundedRectangle(b, rad);
    g.setColour(on ? accent : theme::border);
    g.drawRoundedRectangle(b, rad, 1.f);
    auto inner = b.reduced(b.getHeight() * 0.3f, 0.f);
    const float dot = b.getHeight() * 0.28f;
    auto dotArea = inner.removeFromLeft(dot).withSizeKeepingCentre(dot, dot);
    const bool lit = showLed && juce::Time::getMillisecondCounter() < ledUntil;
    g.setColour(lit ? juce::Colours::white : on ? accent : theme::faint);
    g.fillEllipse(dotArea);
    inner.removeFromLeft(b.getHeight() * 0.2f);
    g.setColour(on ? theme::text : theme::dim);
    g.setFont(theme::font(b.getHeight() * 0.5f, on));
    g.drawFittedText(text, inner.toNearestInt(), juce::Justification::centredLeft, 1);
}

void Toggle::mouseDown(const juce::MouseEvent&) { attachment->setValueAsCompleteGesture(value >= 0.5f ? 0.f : 1.f); }

// ---- PowerButton --------------------------------------------------------------------------------
PowerButton::PowerButton(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour)
    : ParamControl(s, paramId, accentColour) { setTooltip("Module on/off"); }

void PowerButton::paint(juce::Graphics& g)
{
    const bool active = value < 0.5f; // bypass off
    auto b = getLocalBounds().toFloat();
    const float d = std::min(b.getWidth(), b.getHeight()) * 0.9f;
    auto c = juce::Rectangle<float>(d, d).withCentre(b.getCentre());
    g.setColour(active ? accent.withAlpha(0.2f) : theme::panelHi);
    g.fillEllipse(c);
    g.setColour(active ? accent : theme::faint);
    g.drawEllipse(c, 1.f);
    // power glyph
    const float r = d * 0.26f, cx = c.getCentreX(), cy = c.getCentreY();
    juce::Path p;
    p.addCentredArc(cx, cy, r, r, 0.f, 0.7f, juce::MathConstants<float>::twoPi - 0.7f, true);
    p.startNewSubPath(cx, cy - r * 1.25f);
    p.lineTo(cx, cy - r * 0.2f);
    g.strokePath(p, juce::PathStrokeType(std::max(1.5f, d * 0.08f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PowerButton::mouseDown(const juce::MouseEvent&) { attachment->setValueAsCompleteGesture(value >= 0.5f ? 0.f : 1.f); }

// ---- Segmented ----------------------------------------------------------------------------------
Segmented::Segmented(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour, juce::StringArray l)
    : ParamControl(s, paramId, accentColour), labels(std::move(l))
{
    if (labels.isEmpty())
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(&param)) labels = c->choices;
}

void Segmented::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.f);
    const int n = labels.size(), sel = juce::roundToInt(value);
    const float rad = b.getHeight() * 0.25f;
    g.setColour(theme::panelHi);
    g.fillRoundedRectangle(b, rad);
    g.setColour(theme::border);
    g.drawRoundedRectangle(b, rad, 1.f);
    const float w = b.getWidth() / (float) n;
    g.setFont(theme::font(b.getHeight() * 0.46f));
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float>(b.getX() + w * i, b.getY(), w, b.getHeight());
        if (i == sel)
        {
            g.setColour(accent.withAlpha(0.22f));
            g.fillRoundedRectangle(seg.reduced(2.f), rad * 0.8f);
            g.setColour(accent);
            g.drawRoundedRectangle(seg.reduced(2.f), rad * 0.8f, 1.f);
        }
        g.setColour(i == sel ? theme::text : theme::dim);
        g.drawFittedText(labels[i], seg.reduced(3.f, 0.f).toNearestInt(), juce::Justification::centred, 1);
    }
}

void Segmented::mouseDown(const juce::MouseEvent& e)
{
    const int n = labels.size();
    const int idx = juce::jlimit(0, n - 1, (int) (e.position.x / (getWidth() / (float) n)));
    attachment->setValueAsCompleteGesture((float) idx);
}

// ---- ChoiceMenu ---------------------------------------------------------------------------------
ChoiceMenu::ChoiceMenu(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::Colour accentColour)
    : ParamControl(s, paramId, accentColour) {}

void ChoiceMenu::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.f);
    const float rad = b.getHeight() * 0.25f;
    g.setColour(theme::panelHi);
    g.fillRoundedRectangle(b, rad);
    g.setColour(theme::border);
    g.drawRoundedRectangle(b, rad, 1.f);
    auto inner = b.reduced(b.getHeight() * 0.35f, 0.f);
    auto arrow = inner.removeFromRight(b.getHeight() * 0.4f);
    juce::Path tri;
    tri.addTriangle(arrow.getX(), arrow.getCentreY() - arrow.getWidth() * 0.25f, arrow.getRight(), arrow.getCentreY() - arrow.getWidth() * 0.25f,
                    arrow.getCentreX(), arrow.getCentreY() + arrow.getWidth() * 0.3f);
    g.setColour(accent);
    g.fillPath(tri);
    g.setColour(theme::text);
    g.setFont(theme::font(b.getHeight() * 0.48f));
    g.drawFittedText(param.getCurrentValueAsText(), inner.toNearestInt(), juce::Justification::centredLeft, 1);
}

void ChoiceMenu::mouseDown(const juce::MouseEvent&)
{
    auto* c = dynamic_cast<juce::AudioParameterChoice*>(&param);
    if (! c) return;
    juce::PopupMenu m;
    for (int i = 0; i < c->choices.size(); ++i) m.addItem(i + 1, c->choices[i], true, i == juce::roundToInt(value));
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                    [sp = juce::Component::SafePointer<ChoiceMenu>(this)](int r) {
                        if (sp && r > 0) sp->attachment->setValueAsCompleteGesture((float) (r - 1));
                    });
}
