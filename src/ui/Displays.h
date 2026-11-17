#pragma once
#include "Theme.h"

// Stereo peak meter, -60..+6 dBFS. Fed by the editor timer (no audio-thread access here).
class LevelMeter : public juce::Component
{
public:
    void setPeaks(float l, float r)
    {
        for (int c = 0; c < 2; ++c)
        {
            const float pk = c == 0 ? l : r;
            level[c] = std::max(pk, level[c] * 0.85f); // ~0.5 s fall at 30 Hz
            if (pk >= hold[c]) { hold[c] = pk; holdTicks[c] = 45; }
            else if (--holdTicks[c] <= 0) hold[c] = std::max(pk, hold[c] * 0.9f);
        }
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float labelW = b.getWidth() * 0.42f;
        auto scale = b.removeFromLeft(labelW);
        const float gap = b.getWidth() * 0.12f, barW = (b.getWidth() - gap) * 0.5f;
        auto yOf = [&](float db) { return b.getY() + b.getHeight() * (1.f - (juce::jlimit(-60.f, 6.f, db) + 60.f) / 66.f); };
        g.setFont(theme::font(std::max(8.f, labelW * 0.42f)));
        for (float db : { 0.f, -6.f, -12.f, -24.f, -48.f })
        {
            const float y = yOf(db);
            g.setColour(theme::faint);
            g.drawFittedText(juce::String((int) db), scale.withY(y - 6.f).withHeight(12.f).toNearestInt().withTrimmedRight(3), juce::Justification::centredRight, 1);
            g.setColour(theme::border);
            g.drawHorizontalLine((int) y, b.getX(), b.getRight());
        }
        for (int c = 0; c < 2; ++c)
        {
            auto bar = juce::Rectangle<float>(b.getX() + c * (barW + gap), b.getY(), barW, b.getHeight());
            g.setColour(theme::panelHi);
            g.fillRect(bar);
            const float db = 20.f * std::log10(std::max(level[c], 1.0e-6f));
            const float y = yOf(db);
            auto fill = bar.withTop(y);
            g.setColour(db > -0.1f ? juce::Colour(0xffe5534b) : db > -6.f ? juce::Colour(0xffe3c552) : juce::Colour(0xff6cc58a));
            g.fillRect(fill);
            const float hdb = 20.f * std::log10(std::max(hold[c], 1.0e-6f));
            if (hdb > -60.f)
            {
                g.setColour(hdb > -0.1f ? juce::Colour(0xffe5534b) : theme::text);
                g.fillRect(bar.withY(yOf(hdb) - 1.f).withHeight(2.f));
            }
        }
    }
private:
    float level[2] {}, hold[2] {};
    int holdTicks[2] {};
};

// Scrolling trace of the SHAPE gain (dB), about the last 2 seconds.
class GainTrace : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit GainTrace(juce::Colour c) : colour(c) { setTooltip("Gain the transient shaper is applying right now"); }
    void push(const float* v, int n)
    {
        for (int i = 0; i < n; ++i) { buf[(size_t) w] = v[i]; w = (w + 1) % kLen; }
        if (n > 0) repaint();
    }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(theme::bg);
        g.fillRoundedRectangle(b, 4.f);
        g.setColour(theme::border);
        g.drawRoundedRectangle(b.reduced(0.5f), 4.f, 1.f);
        auto plot = b.reduced(4.f, 5.f);
        const float range = 15.f, cy = plot.getCentreY();
        g.setColour(theme::track);
        g.drawHorizontalLine((int) cy, plot.getX(), plot.getRight());
        juce::Path line, area;
        for (int i = 0; i < kLen; ++i)
        {
            const float db = buf[(size_t) ((w + i) % kLen)];
            const float x = plot.getX() + plot.getWidth() * (float) i / (kLen - 1);
            const float y = cy - juce::jlimit(-1.f, 1.f, db / range) * plot.getHeight() * 0.5f;
            if (i == 0) { line.startNewSubPath(x, y); area.startNewSubPath(x, cy); }
            line.lineTo(x, y); area.lineTo(x, y);
        }
        area.lineTo(plot.getRight(), cy);
        area.closeSubPath();
        g.setColour(colour.withAlpha(0.18f));
        g.fillPath(area);
        g.setColour(colour);
        g.strokePath(line, juce::PathStrokeType(1.4f));
        g.setColour(theme::faint);
        g.setFont(theme::font(9.5f * theme::scaleOf(*this)));
        g.drawText("+15", plot.toNearestInt(), juce::Justification::topLeft);
        g.drawText("-15", plot.toNearestInt(), juce::Justification::bottomLeft);
        g.drawText("gain", plot.toNearestInt(), juce::Justification::topRight);
    }
private:
    static constexpr int kLen = 400;
    std::array<float, kLen> buf {};
    int w = 0;
    juce::Colour colour;
};
