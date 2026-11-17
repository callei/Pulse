#include "ModuleCard.h"
#include "../Params.h"

ModuleCard::ModuleCard(juce::AudioProcessorValueTreeState& s, CardSpec sp)
    : apvts(s), spec(std::move(sp)), accent(theme::accent(spec.module))
{
    power = std::make_unique<PowerButton>(apvts, P::def(spec.bypassId).id, accent);
    power->onChange = [this](float) { updateDimming(); };
    addAndMakeVisible(*power);

    moreButton.setButtonText("More");
    moreButton.setTooltip("Show or hide all controls of this module");
    moreButton.onClick = [this] { setExpanded(! expanded); };
    addAndMakeVisible(moreButton);
    addAndMakeVisible(content);
    content.setInterceptsMouseClicks(false, true);

    for (auto& h : spec.heroes)
    {
        make(h.id, h.label);
        if (h.dependsOn >= 0) dependsOn[h.id] = h.dependsOn;
    }
    for (int id : spec.compact) make(id);
    for (auto& sec : spec.detail)
    {
        if (sec.toggleId >= 0) make(sec.toggleId);
        for (int id : sec.items)
        {
            make(id);
            if (sec.toggleId >= 0) dependsOn[id] = sec.toggleId;
        }
    }
    for (auto& [id, dep] : dependsOn)
        if (auto* t = control(dep)) t->onChange = [this](float) { updateDimming(); };
    updateDimming();
    setExpanded(false);
}

ParamControl& ModuleCard::make(int id, const char* labelOverride)
{
    if (auto* existing = control(id)) return *existing;
    const auto& d = P::def(id);
    std::unique_ptr<ParamControl> c;
    if (P::isBool(id)) c = std::make_unique<Toggle>(apvts, d.id, accent, d.label);
    else if (P::isChoice(id) && d.choices.size() <= 3) c = std::make_unique<Segmented>(apvts, d.id, accent);
    else if (P::isChoice(id)) c = std::make_unique<ChoiceMenu>(apvts, d.id, accent);
    else c = std::make_unique<Knob>(apvts, d.id, accent, labelOverride ? juce::String(labelOverride) : juce::String());
    content.addAndMakeVisible(*c);
    auto& ref = *c;
    ctrls[id] = std::move(c);
    return ref;
}

bool ModuleCard::isSectionToggle(int id) const
{
    for (auto& s : spec.detail) if (s.toggleId == id) return true;
    return false;
}

void ModuleCard::updateDimming()
{
    content.setAlpha(power->plain() >= 0.5f ? 0.38f : 1.f);
    for (auto& [id, dep] : dependsOn)
        if (auto* c = control(id); c && control(dep)) c->setAlpha(control(dep)->plain() >= 0.5f ? 1.f : 0.45f);
    repaint();
}

void ModuleCard::setExpanded(bool e)
{
    expanded = e;
    moreButton.setButtonText(expanded ? "Less" : "More");
    const auto inCompact = [this](int id) { return std::find(spec.compact.begin(), spec.compact.end(), id) != spec.compact.end(); };
    for (auto& s : spec.detail)
    {
        for (int id : s.items) if (auto* c = control(id)) c->setVisible(expanded);
        if (s.toggleId >= 0 && ! inCompact(s.toggleId)) if (auto* c = control(s.toggleId)) c->setVisible(expanded);
    }
    resized();
    repaint();
    if (onExpandChanged) onExpandChanged(expanded);
}

void ModuleCard::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float rad = 8.f * getHeight() / 560.f;
    g.setColour(theme::panel);
    g.fillRoundedRectangle(b, rad);
    g.setColour(theme::border);
    g.drawRoundedRectangle(b.reduced(0.5f), rad, 1.f);
    // accent rule along the top: the only colour cue per module, dimmed when off
    const bool on = power->plain() < 0.5f;
    g.setColour(on ? accent : theme::faint);
    g.fillRoundedRectangle(b.withHeight(3.f).reduced(rad, 0.f), 1.5f);

    const float s = getHeight() / 560.f;
    auto header = b.withHeight(headerH).withTrimmedLeft(headerH + 12.f * s);
    g.setColour(on ? theme::text : theme::dim);
    g.setFont(theme::font(15.f * s, true));
    g.drawText(spec.title, header.withTrimmedRight(70.f * s).toNearestInt(), juce::Justification::centredLeft);

    // section titles in the detail area
    g.setFont(theme::font(10.5f * s, true));
    g.setColour(theme::dim);
    for (auto& [r, t] : titles) g.drawText(t, r, juce::Justification::centredLeft);
}

void ModuleCard::resized()
{
    const float s = getHeight() / 560.f;
    const int pad = juce::roundToInt(10.f * s), gap = juce::roundToInt(7.f * s);
    headerH = 36.f * s;
    auto b = getLocalBounds();
    auto header = b.removeFromTop(juce::roundToInt(headerH)).reduced(pad, 0).withTrimmedTop(juce::roundToInt(3.f * s));
    power->setBounds(header.removeFromLeft(header.getHeight()).reduced(juce::roundToInt(4.f * s)));
    moreButton.setBounds(header.removeFromRight(juce::roundToInt(52.f * s)).reduced(0, juce::roundToInt(7.f * s)));
    content.setBounds(b.reduced(pad, 0).withTrimmedBottom(pad));

    titles.clear();
    auto area = content.getLocalBounds();
    const int W = area.getWidth();
    const int rowH = juce::roundToInt((expanded ? 26.f : 30.f) * s), pillH = juce::roundToInt((expanded ? 24.f : 28.f) * s);

    // heroes
    const int heroH = juce::roundToInt((expanded ? 82.f : 170.f) * s);
    auto heroRow = area.removeFromTop(heroH);
    const int nh = (int) spec.heroes.size();
    for (int i = 0; i < nh; ++i)
    {
        auto cell = heroRow.withX(heroRow.getX() + i * W / nh).withWidth(W / nh);
        const int kw = std::min(cell.getWidth(), juce::roundToInt((expanded ? 76.f : 130.f) * s));
        if (auto* c = control(spec.heroes[(size_t) i].id)) c->setBounds(cell.withSizeKeepingCentre(kw, heroH));
    }
    area.removeFromTop(gap);

    // compact controls: choices one per row, toggles two per row
    std::vector<ParamControl*> toggles;
    for (int id : spec.compact)
    {
        auto* c = control(id);
        if (! c) continue;
        if (expanded && isSectionToggle(id)) continue;
        if (P::isBool(id)) { toggles.push_back(c); continue; }
        c->setBounds(area.removeFromTop(rowH));
        area.removeFromTop(gap);
    }
    for (size_t i = 0; i < toggles.size(); i += 2)
    {
        auto row = area.removeFromTop(pillH);
        toggles[i]->setBounds(row.removeFromLeft((W - gap) / 2));
        if (i + 1 < toggles.size()) toggles[i + 1]->setBounds(row.withTrimmedLeft(gap));
        area.removeFromTop(gap);
    }

    if (extra)
    {
        const int eh = juce::roundToInt((expanded ? 52.f : 150.f) * s);
        extra->setBounds(area.removeFromTop(eh));
        area.removeFromTop(gap);
    }
    if (! expanded) { repaint(); return; }

    // detail sections: header row = section toggle (or title) + choices/toggles, then a row of knobs
    // knob rows share whatever height is left (GRIT has five sections)
    int knobRows = 0, fixedH = 0;
    for (auto& sec : spec.detail)
    {
        fixedH += juce::roundToInt(4.f * s) + pillH;
        bool hasKnob = false;
        for (int id : sec.items) hasKnob |= ! (P::isBool(id) || P::isChoice(id));
        if (hasKnob) { ++knobRows; fixedH += juce::roundToInt(3.f * s); }
    }
    const int knobH = knobRows == 0 ? 0 : juce::jlimit(juce::roundToInt(40.f * s), juce::roundToInt(62.f * s), (area.getHeight() - fixedH) / knobRows);
    for (auto& sec : spec.detail)
    {
        area.removeFromTop(juce::roundToInt(4.f * s));
        std::vector<ParamControl*> head, knobs;
        for (int id : sec.items)
            if (auto* c = control(id)) (P::isBool(id) || P::isChoice(id) ? head : knobs).push_back(c);
        auto headRow = area.removeFromTop(pillH);
        if (sec.toggleId >= 0)
        {
            if (auto* t = control(sec.toggleId)) t->setBounds(headRow.removeFromLeft(head.empty() ? W / 2 : (W - gap) * 2 / 5));
        }
        else
            titles.emplace_back(headRow.removeFromLeft(head.empty() ? W : (W - gap) * 2 / 5) + content.getPosition(),
                                juce::String(sec.title).toUpperCase());
        if (! head.empty())
        {
            headRow.removeFromLeft(gap);
            const int hw = (headRow.getWidth() - gap * ((int) head.size() - 1)) / (int) head.size();
            for (auto* c : head) { c->setBounds(headRow.removeFromLeft(hw)); headRow.removeFromLeft(gap); }
        }
        if (! knobs.empty())
        {
            area.removeFromTop(juce::roundToInt(3.f * s));
            auto kr = area.removeFromTop(knobH);
            const int cw = W / std::max(3, (int) knobs.size());
            for (auto* c : knobs) c->setBounds(kr.removeFromLeft(cw).withSizeKeepingCentre(std::min(cw, juce::roundToInt(68.f * s)), knobH));
        }
    }
}
