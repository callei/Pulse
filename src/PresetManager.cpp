#include "PresetManager.h"

using namespace P;

const std::vector<PresetManager::Factory>& PresetManager::factory()
{
    static const std::vector<Factory> f {
        { "Init", {} },
        { "Tight Kick", {
            { TapeDrive, 4.f }, { TapeBias, 55.f }, { TapeWow, 4.f }, { TapeFlutter, 4.f },
            { RetroBypass, 1.f },
            { TransMode, 0.f }, { TransAlgo, 1.f }, { TransAttack, 45.f }, { TransSustain, -40.f },
            { TransMojoType, 0.f }, { TransMojoAmt, 30.f },
            { FxBoomOn, 1.f }, { FxBoomAmt, 35.f }, { FxBoomPitch, 50.f }, { FxBoomDecay, 260.f },
            { FxSatOn, 1.f }, { FxSatMode, 0.f }, { FxSatAmt, 20.f }, { FxMoreAmt, 25.f }, { FxAir, 1.f } } },
        { "Fat Snare", {
            { TapeDrive, 8.f }, { TapeFormula, 1.f }, { TapeSpeed, 1.f }, { TapeWow, 10.f }, { TapeFlutter, 8.f },
            { RetroBypass, 1.f },
            { TransMode, 1.f }, { TransAlgo, 0.f }, { TransAttack, 35.f }, { TransSustain, 30.f },
            { TransMojoType, 1.f }, { TransMojoAmt, 40.f },
            { FxSatOn, 1.f }, { FxSatMode, 3.f }, { FxSatAmt, 35.f }, { FxMoreAmt, 30.f }, { FxAir, 2.5f } } },
        { "Bus Glue", {
            { TapeDrive, 3.f }, { TapeSpeed, 2.f }, { TapeBias, 55.f }, { TapeWow, 5.f }, { TapeFlutter, 4.f }, { TapeMix, 70.f },
            { RetroBypass, 1.f },
            { TransMode, 2.f }, { TransAlgo, 2.f }, { TransAttack, 15.f }, { TransSustain, -10.f },
            { TransMojoType, 2.f }, { TransMojoAmt, 20.f }, { TransMix, 70.f },
            { FxSatOn, 1.f }, { FxSatMode, 1.f }, { FxSatAmt, 20.f }, { FxMoreAmt, 35.f }, { FxAir, 1.5f },
            { MasterWidth, 110.f } } },
        { "Lo-Fi Loop", {
            { TapeDrive, 10.f }, { TapeSpeed, 3.f }, { TapeFormula, 2.f }, { TapeWow, 40.f }, { TapeFlutter, 30.f }, { TapeHiss, -62.f },
            { RetroMagnitude, 70.f },
            { RetroNoiseOn, 1.f }, { RetroNoiseType, 0.f }, { RetroNoiseAmt, 30.f }, { RetroNoiseTone, -3.f },
            { RetroWobbleOn, 1.f }, { RetroWobbleAmt, 30.f },
            { RetroDigiOn, 1.f }, { RetroDigiAmt, 35.f }, { RetroDigiBits, 10.f }, { RetroDigiSmooth, 50.f },
            { RetroMagOn, 1.f }, { RetroMagAmt, 35.f }, { RetroMagDropouts, 10.f },
            { TransMode, 2.f }, { TransAlgo, 0.f }, { TransAttack, 20.f }, { TransMojoOn, 0.f },
            { FxSatOn, 1.f }, { FxSatMode, 1.f }, { FxSatAmt, 25.f }, { FxMoreAmt, 20.f }, { FxAir, -3.f },
            { MasterWidth, 85.f } } },
        { "Tape Warmth", {
            { TapeDrive, 10.f }, { TapeFormula, 1.f }, { TapeSpeed, 1.f }, { TapeBias, 60.f }, { TapeWow, 12.f }, { TapeFlutter, 10.f },
            { TapeHiss, -80.f },
            { RetroBypass, 1.f }, { TransBypass, 1.f },
            { FxSatOn, 1.f }, { FxSatMode, 1.f }, { FxSatAmt, 15.f }, { FxMoreAmt, 15.f }, { FxAir, 0.5f } } },
        { "Subtle Polish", {
            { TapeDrive, 2.f }, { TapeSpeed, 2.f }, { TapeWow, 0.f }, { TapeFlutter, 0.f }, { TapeMix, 50.f },
            { RetroBypass, 1.f },
            { TransMode, 2.f }, { TransAlgo, 1.f }, { TransAttack, 10.f }, { TransMojoType, 2.f }, { TransMojoAmt, 15.f },
            { FxSatOn, 1.f }, { FxSatMode, 0.f }, { FxSatAmt, 10.f }, { FxMoreAmt, 20.f }, { FxAir, 2.f } } },
    };
    return f;
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& s) : apvts(s)
{
    loaded = defaultValues();
}

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Pulse").getChildFile("Presets");
}

juce::StringArray PresetManager::userNames() const
{
    juce::StringArray names;
    for (const auto& f : userFolder().findChildFiles(juce::File::findFiles, false, "*.pulsepreset"))
        names.add(f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

P::Values PresetManager::capture() const
{
    Values v {};
    for (int i = 0; i < Count; ++i)
    {
        auto* p = apvts.getParameter(def(i).id);
        v[(size_t) i] = p->convertFrom0to1(p->getValue());
    }
    return v;
}

void PresetManager::apply(const Values& v, const juce::String& name)
{
    if (apvts.undoManager) apvts.undoManager->beginNewTransaction("Load " + name);
    for (int i = 0; i < Count; ++i)
    {
        if (! isPresetParam(i)) continue;
        auto* p = apvts.getParameter(def(i).id);
        const float n = p->convertTo0to1(v[(size_t) i]);
        if (std::abs(n - p->getValue()) < 1.0e-7f) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost(n);
        p->endChangeGesture();
    }
    apvts.state.setProperty("presetName", name, nullptr);
    loaded = capture(); // as stored by the parameters (ranges/steps applied), so "*" is exact
}

bool PresetManager::load(int index)
{
    const auto& f = factory();
    const int nf = (int) f.size();
    if (index < 0) return false;
    if (index < nf)
    {
        Values v = defaultValues();
        for (auto [id, val] : f[(size_t) index].values) v[(size_t) id] = val;
        currentIndex = index;
        apply(v, f[(size_t) index].name);
        return true;
    }
    const auto names = userNames();
    if (index - nf >= names.size()) return false;
    const auto name = names[index - nf];
    auto xml = juce::XmlDocument::parse(userFolder().getChildFile(name + ".pulsepreset"));
    if (! xml || ! xml->hasTagName("PulsePreset")) return false;
    Values v = defaultValues();
    for (auto* e : xml->getChildWithTagNameIterator("Param"))
    {
        const int id = findByIdString(e->getStringAttribute("id"));
        if (id >= 0) v[(size_t) id] = (float) e->getDoubleAttribute("value", v[(size_t) id]);
    }
    currentIndex = index;
    apply(v, name);
    return true;
}

void PresetManager::step(int delta)
{
    const int n = numPresets();
    for (int tries = 1; tries <= n; ++tries) // skip unreadable user preset files
    {
        const int idx = ((currentIndex + delta * tries) % n + n) % n;
        if (load(idx)) return;
    }
}

bool PresetManager::saveUser(const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName(rawName.trim());
    if (name.isEmpty()) return false;
    const auto v = capture();
    juce::XmlElement xml("PulsePreset");
    xml.setAttribute("name", name);
    for (int i = 0; i < Count; ++i)
    {
        if (! isPresetParam(i)) continue;
        auto* e = xml.createNewChildElement("Param");
        e->setAttribute("id", def(i).id);
        e->setAttribute("value", v[(size_t) i]);
    }
    const auto folder = userFolder();
    if (! folder.createDirectory()) return false;
    if (! xml.writeTo(folder.getChildFile(name + ".pulsepreset"))) return false;
    apvts.state.setProperty("presetName", name, nullptr);
    loaded = v;
    const int idx = userNames().indexOf(name);
    if (idx >= 0) currentIndex = (int) factory().size() + idx;
    return true;
}

juce::String PresetManager::currentName() const
{
    return apvts.state.getProperty("presetName", "Init").toString();
}

bool PresetManager::isModified() const
{
    const auto v = capture();
    for (int i = 0; i < Count; ++i)
        if (isPresetParam(i) && std::abs(v[(size_t) i] - loaded[(size_t) i]) > 1.0e-4f * (1.f + std::abs(loaded[(size_t) i])))
            return true;
    return false;
}

void PresetManager::markLoaded() { loaded = capture(); }

void PresetManager::selectSlot(int s)
{
    if (s == slot || s < 0 || s > 1) return;
    slots[slot] = capture();
    slotNames[slot] = currentName();
    slotValid[slot] = true;
    slot = s;
    if (slotValid[s]) apply(slots[s], slotNames[s]); // first visit to a slot starts as a copy of the other
}

void PresetManager::copyActiveToOther()
{
    const int other = 1 - slot;
    slots[other] = capture();
    slotNames[other] = currentName();
    slotValid[other] = true;
}
