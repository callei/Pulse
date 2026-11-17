// pulse_uishot out.png [--width N] [--open MASK] [--preset INDEX] [Name=value ...]   |   pulse_uishot --selftest
// Renders the Pulse editor off-screen to a PNG (developer tool for checking the layout).
// MASK: bit 0..3 = TAPE, GRIT, SHAPE, PUNCH expanded.
#include "../src/PluginEditor.h"

static float plain(PulseProcessor& p, int id)
{
    auto* q = p.apvts.getParameter(P::def(id).id);
    return q->convertFrom0to1(q->getValue());
}
static void set(PulseProcessor& p, int id, float v)
{
    auto* q = p.apvts.getParameter(P::def(id).id);
    q->setValueNotifyingHost(q->convertTo0to1(v));
}
static int fails = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++fails; }

// Presets, A/B, undo and state recall, exercised through the same objects the editor uses.
static int selfTest(PulseProcessor& p)
{
    auto& pm = p.presets;
    auto flush = [&] { for (int i = 0; i < 3; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(60); };
    // factory presets
    const auto& f = PresetManager::factory();
    pm.load(1);
    check(pm.currentName() == f[1].name && std::abs(plain(p, P::TransAttack) - 45.f) < 0.01f, "factory preset 'Tight Kick' loads its values");
    check(! pm.isModified(), "freshly loaded preset is not marked modified");
    set(p, P::TapeDrive, 12.f);
    check(pm.isModified(), "changing a knob marks the preset modified");
    pm.step(1);
    check(pm.currentName() == f[2].name, "next preset steps to 'Fat Snare'");
    // Quality / Match are not part of presets
    set(p, P::Quality, 0.f); set(p, P::AutoGain, 1.f);
    pm.load(0);
    check(plain(p, P::Quality) < 0.5f && plain(p, P::AutoGain) > 0.5f, "presets leave Quality and Match alone");
    // A/B
    pm.load(4); // Lo-Fi Loop
    const float aWow = plain(p, P::TapeWow);
    pm.selectSlot(1);
    check(std::abs(plain(p, P::TapeWow) - aWow) < 0.01f, "B starts as a copy of A");
    set(p, P::TapeWow, 90.f);
    pm.selectSlot(0);
    check(std::abs(plain(p, P::TapeWow) - aWow) < 0.01f, "switching back to A restores A");
    pm.selectSlot(1);
    check(std::abs(plain(p, P::TapeWow) - 90.f) < 0.01f, "switching to B restores B");
    pm.selectSlot(0);
    // user preset round trip
    const auto tmpName = juce::String("zz_selftest_") + juce::String(juce::Random::getSystemRandom().nextInt(100000));
    set(p, P::FxBoomPitch, 72.f);
    check(pm.saveUser(tmpName), "user preset saves");
    set(p, P::FxBoomPitch, 40.f);
    pm.load((int) f.size() + pm.userNames().indexOf(tmpName));
    check(std::abs(plain(p, P::FxBoomPitch) - 72.f) < 0.05f && pm.currentName() == tmpName, "user preset loads back");
    PresetManager::userFolder().getChildFile(tmpName + ".pulsepreset").deleteFile();
    // undo / redo (APVTS writes to the tree on its timer, so let the message loop run)
    p.undo.clearUndoHistory();
    flush();
    p.undo.beginNewTransaction();
    set(p, P::TransSustain, -55.f);
    flush();
    p.undo.beginNewTransaction();
    check(p.undo.canUndo(), "a knob change can be undone");
    p.undo.undo();
    flush();
    check(std::abs(plain(p, P::TransSustain) - (-55.f)) > 0.5f, "undo restores the previous value");
    p.undo.redo();
    flush();
    check(std::abs(plain(p, P::TransSustain) - (-55.f)) < 0.05f, "redo re-applies it");
    // state save / recall incl. seed and window size
    p.uiWidth = 1300;
    juce::MemoryBlock mb;
    p.getStateInformation(mb);
    PulseProcessor q;
    q.setStateInformation(mb.getData(), (int) mb.getSize());
    bool same = true;
    for (int i = 0; i < P::Count; ++i) same &= std::abs(plain(p, i) - plain(q, i)) < 1.0e-4f;
    check(same, "all parameters survive save/recall");
    check(q.uiWidth == 1300 && q.presets.currentName() == p.presets.currentName(), "window size and preset name survive save/recall");
    std::printf(fails == 0 ? "ALL PASS\n" : "%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::puts("usage: pulse_uishot out.png [--width N] [--open MASK] [Name=value ...]"); return 1; }
    PulseProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    if (juce::String(argv[1]) == "--selftest") return selfTest(proc);
    int width = PulseEditor::kBaseW, mask = 0;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String a(argv[i]);
        if (a == "--width" && i + 1 < argc) width = juce::String(argv[++i]).getIntValue();
        else if (a == "--open" && i + 1 < argc) mask = juce::String(argv[++i]).getIntValue();
        else if (a == "--preset" && i + 1 < argc) proc.presets.load(juce::String(argv[++i]).getIntValue());
        else if (a.contains("="))
        {
            const int id = P::findByIdString(a.upToFirstOccurrenceOf("=", false, false));
            if (id < 0) { std::printf("unknown parameter %s\n", argv[i]); return 1; }
            auto* p = proc.apvts.getParameter(P::def(id).id);
            p->setValueNotifyingHost(p->convertTo0to1(a.fromFirstOccurrenceOf("=", false, false).getFloatValue()));
        }
    }
    for (int i = 0; i < 4; ++i) proc.cardOpen[(size_t) i] = (mask >> i) & 1;
    proc.uiWidth = width;

    // run some audio so meters/trace have something to show
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    for (int b = 0; b < 40; ++b)
    {
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 512; ++n)
            {
                const int t = b * 512 + n;
                buf.setSample(c, n, (t % 12000) < 2400 ? 0.7f * std::sin(t * 0.008f) * std::exp(-(t % 12000) / 600.f) : 0.02f * std::sin(t * 0.05f));
            }
        proc.processBlock(buf, midi);
        if (auto* t = dynamic_cast<juce::Timer*>(ed.get())) t->timerCallback();
    }
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.f);
    juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
    out.deleteFile();
    juce::FileOutputStream os(out);
    juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf("%s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
