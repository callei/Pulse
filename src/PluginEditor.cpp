#include "PluginEditor.h"

using namespace P;

namespace
{
CardSpec tapeSpec()
{
    return { theme::Tape, "TAPE", TapeBypass,
             { { TapeDrive, nullptr, -1 }, { TapeBias, nullptr, -1 } },
             { TapeFormula, TapeSpeed },
             { { "Motion", -1, { TapeWow, TapeFlutter } },
               { "Character", -1, { TapeHeadBump, TapeHiss, TapeMix } } } };
}
CardSpec gritSpec()
{
    return { theme::Grit, "GRIT", RetroBypass,
             { { RetroMagnitude, nullptr, -1 }, { RetroNoiseFlux, nullptr, -1 } },
             { RetroNoiseOn, RetroWobbleOn, RetroDistOn, RetroDigiOn, RetroMagOn },
             { { "Noise", RetroNoiseOn, { RetroNoiseType, RetroNoiseAmt, RetroNoiseTone } },
               { "Wobble", RetroWobbleOn, { RetroWobbleStereo, RetroWobbleAmt, RetroWobbleMix } },
               { "Distort", RetroDistOn, { RetroDistType, RetroDistAmt, RetroDistFocus } },
               { "Digital", RetroDigiOn, { RetroDigiAmt, RetroDigiBits, RetroDigiSmooth } },
               { "Magnetic", RetroMagOn, { RetroMagAmt, RetroMagDropouts } } } };
}
CardSpec shapeSpec()
{
    return { theme::Shape, "SHAPE", TransBypass,
             { { TransAttack, nullptr, -1 }, { TransSustain, nullptr, -1 } },
             { TransMode, TransAlgo, TransMojoOn },
             { { "Mojo", TransMojoOn, { TransMojoType, TransMojoAmt } },
               { "Output", -1, { TransMix } } } };
}
CardSpec punchSpec()
{
    return { theme::Punch, "PUNCH", FxBypass,
             { { FxSatAmt, "Saturate", FxSatOn }, { FxMoreAmt, "Push", FxMoreOn } },
             { FxBoomOn, FxSatOn, FxMoreOn },
             { { "Boom", FxBoomOn, { FxBoomAmt, FxBoomPitch, FxBoomDecay, FxBoomSens } },
               { "Saturate", FxSatOn, { FxSatMode } },
               { "Finish", FxMoreOn, {} },
               { "Tone", -1, { FxAir } } } };
}
} // namespace

PulseEditor::PulseEditor(PulseProcessor& p) : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&lnf);
    tooltips.setLookAndFeel(&lnf);
    auto& s = proc.apvts;

    CardSpec specs[4] = { tapeSpec(), gritSpec(), shapeSpec(), punchSpec() };
    for (int i = 0; i < 4; ++i)
    {
        cards[i] = std::make_unique<ModuleCard>(s, specs[i]);
        cards[i]->setExpanded(proc.cardOpen[(size_t) i]);
        cards[i]->onExpandChanged = [this, i](bool e) { proc.cardOpen[(size_t) i] = e; };
        addAndMakeVisible(*cards[i]);
    }
    cards[theme::Shape]->setExtra(&trace);
    if (auto* boom = dynamic_cast<Toggle*>(cards[theme::Punch]->control(FxBoomOn))) boom->showLed = true;

    const auto n = theme::neutral;
    inputKnob = std::make_unique<Knob>(s, def(InputGain).id, n, "Input");
    outputKnob = std::make_unique<Knob>(s, def(OutputGain).id, n, "Output");
    mixKnob = std::make_unique<Knob>(s, def(MasterMix).id, n, "Mix");
    widthKnob = std::make_unique<Knob>(s, def(MasterWidth).id, n, "Width");
    quality = std::make_unique<Segmented>(s, def(Quality).id, n);
    match = std::make_unique<Toggle>(s, def(AutoGain).id, n, "Match");
    for (juce::Component* c : { (juce::Component*) inputKnob.get(), (juce::Component*) outputKnob.get(), (juce::Component*) mixKnob.get(),
                                (juce::Component*) widthKnob.get(), (juce::Component*) quality.get(), (juce::Component*) match.get(),
                                (juce::Component*) &inMeter, (juce::Component*) &outMeter })
        addAndMakeVisible(c);

    auto& pm = proc.presets;
    prevPreset.onClick = [&pm] { pm.step(-1); };
    nextPreset.onClick = [&pm] { pm.step(1); };
    presetName.onClick = [this] { showPresetMenu(); };
    savePreset.onClick = [this] { savePresetDialog(); };
    slotA.onClick = [&pm] { pm.selectSlot(0); };
    slotB.onClick = [&pm] { pm.selectSlot(1); };
    copyAB.onClick = [&pm] { pm.copyActiveToOther(); };
    undoButton.onClick = [this] { proc.undo.undo(); };
    redoButton.onClick = [this] { proc.undo.redo(); };
    prevPreset.setTooltip("Previous preset");
    nextPreset.setTooltip("Next preset");
    presetName.setTooltip("Choose a preset. * = changed since loading");
    savePreset.setTooltip("Save the current sound as a user preset");
    slotA.setTooltip("Compare: sound A");
    slotB.setTooltip("Compare: sound B (starts as a copy of A)");
    copyAB.setTooltip("Copy the current sound to the other slot");
    undoButton.setTooltip("Undo the last change");
    redoButton.setTooltip("Redo");
    for (auto* b : { &prevPreset, &nextPreset, &presetName, &savePreset, &slotA, &slotB, &copyAB, &undoButton, &redoButton })
        addAndMakeVisible(b);

    const int w = proc.uiWidth > 0 ? juce::jlimit(kBaseW * 7 / 10, kBaseW * 2, proc.uiWidth) : kBaseW; // read before resized() runs
    const double ratio = (double) kBaseW / kBaseH;
    setSize(w, juce::roundToInt(w / ratio));
    setResizable(true, true);
    setResizeLimits(kBaseW * 7 / 10, kBaseH * 7 / 10, kBaseW * 2, kBaseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio(ratio);

    lastBoomCount = proc.engine.telemetry.boomCount.load();
    startTimerHz(30);
}

PulseEditor::~PulseEditor()
{
    stopTimer();
    tooltips.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void PulseEditor::paint(juce::Graphics& g)
{
    g.fillAll(theme::bg);
    const float s = getHeight() / (float) kBaseH;
    g.setColour(theme::text);
    g.setFont(theme::font(20.f * s, true));
    g.drawText("Pulse", juce::Rectangle<float>(16.f * s, 0.f, 90.f * s, 52.f * s).toNearestInt(), juce::Justification::centredLeft);

    g.setFont(theme::font(11.f * s, true));
    g.setColour(theme::dim);
    g.setFont(theme::font(10.5f * s));
    g.drawText("Quality", quality->getBounds().translated(-juce::roundToInt(60.f * s), 0).withWidth(juce::roundToInt(54.f * s)),
               juce::Justification::centredRight);
    if (match->plain() >= 0.5f)
        g.drawText(juce::String(matchDb >= 0 ? "+" : "") + juce::String(matchDb, 1) + " dB",
                   match->getBounds().translated(0, match->getHeight()).withHeight(juce::roundToInt(16.f * s)), juce::Justification::centred);
}

void PulseEditor::resized()
{
    proc.uiWidth = getWidth();
    const float s = getHeight() / (float) kBaseH;
    auto px = [s](float v) { return juce::roundToInt(v * s); };
    auto b = getLocalBounds();

    // header
    auto header = b.removeFromTop(px(52)).reduced(px(14), px(12));
    header.removeFromLeft(px(86));
    prevPreset.setBounds(header.removeFromLeft(px(28)));
    header.removeFromLeft(px(4));
    presetName.setBounds(header.removeFromLeft(px(210)));
    header.removeFromLeft(px(4));
    nextPreset.setBounds(header.removeFromLeft(px(28)));
    header.removeFromLeft(px(8));
    savePreset.setBounds(header.removeFromLeft(px(56)));
    header.removeFromLeft(px(22));
    slotA.setBounds(header.removeFromLeft(px(30)));
    header.removeFromLeft(px(4));
    slotB.setBounds(header.removeFromLeft(px(30)));
    header.removeFromLeft(px(4));
    copyAB.setBounds(header.removeFromLeft(px(52)));
    header.removeFromLeft(px(22));
    undoButton.setBounds(header.removeFromLeft(px(56)));
    header.removeFromLeft(px(4));
    redoButton.setBounds(header.removeFromLeft(px(56)));
    quality->setBounds(header.removeFromRight(px(120)));

    auto body = b.reduced(px(12), 0).withTrimmedBottom(px(12)).withTrimmedTop(px(18));
    const int colW = px(88), gap = px(10);

    // IN column
    auto in = body.removeFromLeft(colW);
    inputKnob->setBounds(in.removeFromTop(px(92)));
    in.removeFromTop(gap);
    inMeter.setBounds(in.withSizeKeepingCentre(px(52), in.getHeight()));
    body.removeFromLeft(gap);

    // OUT column (signal order: width, mix, match, output)
    auto out = body.removeFromRight(colW);
    widthKnob->setBounds(out.removeFromTop(px(80)));
    out.removeFromTop(px(4));
    mixKnob->setBounds(out.removeFromTop(px(80)));
    out.removeFromTop(px(8));
    match->setBounds(out.removeFromTop(px(24)).reduced(px(6), 0));
    out.removeFromTop(px(20));
    outputKnob->setBounds(out.removeFromTop(px(92)));
    out.removeFromTop(gap);
    outMeter.setBounds(out.withSizeKeepingCentre(px(52), out.getHeight()));
    body.removeFromRight(gap);

    // module cards in signal order; GRIT gets more room for its five sections
    const float weights[4] = { 1.f, 1.3f, 1.f, 1.15f };
    const float total = weights[0] + weights[1] + weights[2] + weights[3];
    const int avail = body.getWidth() - 3 * gap;
    for (int i = 0; i < 4; ++i)
    {
        const int w = i == 3 ? body.getWidth() : juce::roundToInt(avail * weights[i] / total);
        cards[i]->setBounds(body.removeFromLeft(w));
        body.removeFromLeft(gap);
    }
}

void PulseEditor::timerCallback()
{
    auto& gl = proc.engine.global;
    inMeter.setPeaks(gl.inPeak[0].exchange(0.f), gl.inPeak[1].exchange(0.f));
    outMeter.setPeaks(gl.outPeak[0].exchange(0.f), gl.outPeak[1].exchange(0.f));

    float tmp[256];
    for (int got; (got = proc.engine.telemetry.pull(tmp, 256)) > 0;) trace.push(tmp, got);

    const int bc = proc.engine.telemetry.boomCount.load();
    if (bc != lastBoomCount)
    {
        lastBoomCount = bc;
        if (auto* boom = dynamic_cast<Toggle*>(cards[theme::Punch]->control(FxBoomOn))) boom->flash();
    }
    else if (auto* boom = cards[theme::Punch]->control(FxBoomOn)) boom->repaint(); // let the LED go dark

    undoButton.setEnabled(proc.undo.canUndo());
    redoButton.setEnabled(proc.undo.canRedo());
    auto& pm = proc.presets;
    if (proc.stateRecalled.exchange(false)) pm.markLoaded();
    const auto name = pm.currentName() + (pm.isModified() ? " *" : "");
    if (presetName.getButtonText() != name) presetName.setButtonText(name);
    slotA.setToggleState(pm.activeSlot() == 0, juce::dontSendNotification);
    slotB.setToggleState(pm.activeSlot() == 1, juce::dontSendNotification);

    const float m = gl.autoGainDb.load();
    if (std::abs(m - matchDb) > 0.05f) { matchDb = m; repaint(match->getBounds().translated(0, match->getHeight())); }
}

void PulseEditor::showPresetMenu()
{
    auto& pm = proc.presets;
    const auto& f = PresetManager::factory();
    const auto user = pm.userNames();
    juce::PopupMenu m;
    m.addSectionHeader("Factory");
    for (int i = 0; i < (int) f.size(); ++i) m.addItem(i + 1, f[(size_t) i].name, true, pm.currentName() == f[(size_t) i].name);
    m.addSectionHeader("User");
    if (user.isEmpty()) m.addItem(-1, "(none yet: use Save)", false);
    for (int i = 0; i < user.size(); ++i) m.addItem((int) f.size() + i + 1, user[i], true, pm.currentName() == user[i]);
    m.addSeparator();
    m.addItem(10000, "Save as...");
    m.addItem(10001, "Open presets folder");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetName),
                    [this, sp = juce::Component::SafePointer<PulseEditor>(this)](int r) {
                        if (! sp || r == 0) return;
                        if (r == 10000) savePresetDialog();
                        else if (r == 10001) { PresetManager::userFolder().createDirectory(); PresetManager::userFolder().startAsProcess(); }
                        else if (r > 0) proc.presets.load(r - 1);
                    });
}

void PulseEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow("Save preset", "Name for the new preset:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel(&lnf);
    w->addTextEditor("name", proc.presets.currentName());
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([this, w, sp = juce::Component::SafePointer<PulseEditor>(this)](int r) {
                           if (sp && r == 1 && ! proc.presets.saveUser(w->getTextEditorContents("name")))
                               juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save preset",
                                                                      "Could not save to " + PresetManager::userFolder().getFullPathName());
                       }),
                       true);
}
