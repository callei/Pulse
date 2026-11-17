#include "Params.h"
#include <cstring>

namespace P
{
using U = Unit;
using C = Curve;
static const std::vector<const char*> kOnOff { "Off", "On" };

const std::vector<Def>& defs()
{
    static const std::vector<Def> d {
        { "InputGain",   "Input Gain",   "Input",  -60.f, 12.f, 0.f,   U::dB, C::Linear, {}, "Level into the chain" },
        { "OutputGain",  "Output Gain",  "Output", -60.f, 12.f, 0.f,   U::dB, C::Linear, {}, "Final output level" },
        { "MasterMix",   "Mix",          "Mix",    0.f, 100.f, 100.f,  U::Percent, C::Linear, {}, "Blend of processed and dry signal (dry is time-aligned)" },
        { "MasterWidth", "Width",        "Width",  0.f, 200.f, 100.f,  U::Percent, C::Linear, {}, "Stereo width: 0% mono, 100% unchanged, 200% extra wide" },

        { "TapeBypass",  "Tape Bypass",  "Tape",   0.f, 1.f, 0.f,      U::None, C::Linear, kOnOff, "Bypass the tape module" },
        { "TapeDrive",   "Tape Drive",   "Drive",  -24.f, 24.f, 6.f,   U::dB, C::Linear, {}, "How hard the tape is hit. Level is compensated, so this sets the amount of saturation" },
        { "TapeSpeed",   "Tape Speed",   "Speed",  0.f, 3.f, 1.f,      U::None, C::Linear, { "7.5 ips", "15 ips", "30 ips", "Cassette" }, "Tape speed: sets head-bump frequency and high-frequency roll-off" },
        { "TapeBias",    "Tape Bias",    "Bias",   0.f, 100.f, 50.f,   U::Percent, C::Linear, {}, "Below 50%: brighter, grittier. Above 50%: cleaner, darker" },
        { "TapeFormula", "Tape Formula", "Formula",0.f, 2.f, 0.f,      U::None, C::Linear, { "Modern", "Vintage", "Aggressive" }, "Tape formulation: saturation curve and emphasis" },
        { "TapeWow",     "Tape Wow",     "Wow",    0.f, 100.f, 15.f,   U::Percent, C::Linear, {}, "Slow pitch drift (around 0.8 Hz)" },
        { "TapeFlutter", "Tape Flutter", "Flutter",0.f, 100.f, 12.f,   U::Percent, C::Linear, {}, "Fast pitch jitter (around 12 Hz)" },
        { "TapeHiss",    "Tape Hiss",    "Hiss",   -90.f, -40.f, -90.f,U::dB, C::Linear, {}, "Tape hiss level (RMS). Fully left = off" },
        { "TapeHeadBump","Tape Head Bump","Bump",  0.f, 1.f, 1.f,      U::None, C::Linear, kOnOff, "Low-frequency resonance of the playback head" },
        { "TapeMix",     "Tape Mix",     "Mix",    0.f, 100.f, 100.f,  U::Percent, C::Linear, {}, "Blend of tape and dry signal" },

        { "RetroBypass", "Grit Bypass",  "Grit",   0.f, 1.f, 0.f,      U::None, C::Linear, kOnOff, "Bypass the grit module" },
        { "RetroMagnitude","Grit Intensity","Intensity",0.f,100.f,100.f,U::Percent, C::Linear, {}, "Scales all grit effects together" },
        { "RetroNoiseOn","Grit Noise On","Noise",  0.f, 1.f, 0.f,      U::None, C::Linear, kOnOff, "Enable the noise layer" },
        { "RetroNoiseAmt","Grit Noise Amount","Amount",0.f,100.f,20.f, U::Percent, C::Linear, {}, "Noise level" },
        { "RetroNoiseType","Grit Noise Type","Type",0.f, 3.f, 1.f,     U::None, C::Linear, { "Vinyl", "Tape", "VHS", "Hum" }, "Noise character" },
        { "RetroNoiseTone","Grit Noise Tone","Tone",-10.f, 10.f, 0.f,  U::Bipolar, C::Linear, {}, "Tilt the noise darker (left) or brighter (right)" },
        { "RetroNoiseFlux","Grit Flux",  "Flux",   0.f, 100.f, 25.f,   U::Percent, C::Linear, {}, "Slow random movement of noise, wobble and drive" },
        { "RetroWobbleOn","Grit Wobble On","Wobble",0.f,1.f, 0.f,      U::None, C::Linear, kOnOff, "Enable pitch wobble" },
        { "RetroWobbleAmt","Grit Wobble Amount","Depth",0.f,100.f,25.f,U::Percent, C::Linear, {}, "Wobble depth" },
        { "RetroWobbleStereo","Grit Wobble Stereo","Stereo",0.f,1.f,1.f,U::None, C::Linear, kOnOff, "Different wobble on left and right" },
        { "RetroWobbleMix","Grit Wobble Mix","Mix",0.f, 100.f, 100.f,  U::Percent, C::Linear, {}, "100% = pure pitch wobble, lower = chorus-like blend" },
        { "RetroDistOn", "Grit Distort On","Distort",0.f,1.f, 0.f,     U::None, C::Linear, kOnOff, "Enable band distortion" },
        { "RetroDistAmt","Grit Distort Amount","Drive",0.f,100.f,35.f, U::Percent, C::Linear, {}, "Distortion drive" },
        { "RetroDistType","Grit Distort Type","Type",0.f,4.f, 0.f,     U::None, C::Linear, { "Soft", "Tube", "Diode", "Fuzz", "Transformer" }, "Distortion character" },
        { "RetroDistFocus","Grit Distort Focus","Focus",20.f,20000.f,1500.f,U::Hz, C::Log, {}, "Centre of the frequency band that gets distorted" },
        { "RetroDigiOn", "Grit Digital On","Digital",0.f,1.f, 0.f,     U::None, C::Linear, kOnOff, "Enable bit/sample-rate reduction" },
        { "RetroDigiAmt","Grit Digital Amount","Rate",0.f,100.f,20.f,  U::Percent, C::Linear, {}, "Sample-rate reduction" },
        { "RetroDigiBits","Grit Digital Bits","Bits",2.f, 16.f, 10.f,  U::None, C::Linear, {}, "Bit depth" },
        { "RetroDigiSmooth","Grit Digital Smooth","Smooth",0.f,100.f,40.f,U::Percent, C::Linear, {}, "Softens the digital harshness (low-pass after the reducer)" },
        { "RetroMagOn",  "Grit Magnetic On","Magnetic",0.f,1.f, 0.f,   U::None, C::Linear, kOnOff, "Enable worn-tape effects" },
        { "RetroMagAmt", "Grit Magnetic Amount","Wear",0.f,100.f,25.f, U::Percent, C::Linear, {}, "Tape wear: dulled highs and soft compression" },
        { "RetroMagDropouts","Grit Dropouts","Dropouts",0.f,100.f,15.f,U::Percent, C::Linear, {}, "Random level dropouts" },

        { "TransBypass", "Shape Bypass", "Shape",  0.f, 1.f, 0.f,      U::None, C::Linear, kOnOff, "Bypass the transient shaper" },
        { "TransMode",   "Shape Mode",   "Mode",   0.f, 2.f, 2.f,      U::None, C::Linear, { "Kick", "Snare", "Bus" }, "Detector tuning for the source" },
        { "TransAlgo",   "Shape Algorithm","Character",0.f,2.f,0.f,    U::None, C::Linear, { "Classic", "Natural", "Smooth" }, "Classic: punchy. Natural: look-ahead, cleanest. Smooth: gentle" },
        { "TransAttack", "Shape Attack", "Attack", -100.f, 100.f, 0.f, U::Bipolar, C::Linear, {}, "Boost or soften the hit (up to about 15 dB)" },
        { "TransSustain","Shape Sustain","Sustain",-100.f, 100.f, 0.f, U::Bipolar, C::Linear, {}, "Boost or tighten the tail (up to about 12 dB)" },
        { "TransMojoOn", "Shape Mojo On","Mojo",   0.f, 1.f, 1.f,      U::None, C::Linear, kOnOff, "Enable the character stage" },
        { "TransMojoAmt","Shape Mojo Amount","Amount",0.f,100.f,35.f,  U::Percent, C::Linear, {}, "Character amount" },
        { "TransMojoType","Shape Mojo Type","Type",0.f, 2.f, 0.f,      U::None, C::Linear, { "Punch", "Body", "Air" }, "Punch: upper-mid crack. Body: low weight. Air: smooth highs" },
        { "TransMix",    "Shape Mix",    "Mix",    0.f, 100.f, 100.f,  U::Percent, C::Linear, {}, "Blend of shaped and unshaped signal" },

        { "FxBypass",    "Punch Bypass", "Punch",  0.f, 1.f, 0.f,      U::None, C::Linear, kOnOff, "Bypass the punch module" },
        { "FxBoomOn",    "Punch Boom On","Boom",   0.f, 1.f, 0.f,      U::None, C::Linear, kOnOff, "Add a tuned sub hit on each kick" },
        { "FxBoomAmt",   "Punch Boom Level","Level",0.f, 100.f, 50.f,  U::Percent, C::Linear, {}, "Sub level" },
        { "FxBoomPitch", "Punch Boom Pitch","Pitch",35.f, 110.f, 55.f, U::Hz, C::Log, {}, "Sub pitch" },
        { "FxBoomDecay", "Punch Boom Decay","Decay",50.f, 1200.f, 350.f,U::ms, C::Squared, {}, "Sub length" },
        { "FxSatOn",     "Punch Saturate On","Saturate",0.f,1.f, 0.f,  U::None, C::Linear, kOnOff, "Enable saturation" },
        { "FxSatAmt",    "Punch Saturate Amount","Amount",0.f,100.f,30.f,U::Percent, C::Linear, {}, "Saturation amount" },
        { "FxSatMode",   "Punch Saturate Mode","Mode",0.f, 4.f, 0.f,   U::None, C::Linear, { "Console", "Tape", "Digital", "Tube", "Bloom" }, "Saturation character" },
        { "FxMoreOn",    "Punch Finish On","Finish",0.f, 1.f, 1.f,     U::None, C::Linear, kOnOff, "Loudness finisher with a true-peak safe ceiling" },
        { "FxMoreAmt",   "Punch Finish Amount","Push",0.f,100.f,30.f,  U::Percent, C::Linear, {}, "How hard the finisher pushes" },
        { "FxAir",       "Punch Air",    "Air",    -6.f, 12.f, 1.5f,   U::dB, C::Linear, {}, "Smooth 10 kHz high shelf" },

        { "Quality",     "Quality",      "Quality",0.f, 1.f, 1.f,      U::None, C::Linear, { "Eco", "High" }, "Oversampling: Eco 2x (lighter CPU), High 4x (cleanest)" },
        { "AutoGain",    "Auto Gain",    "Match",  0.f, 1.f, 0.f,      U::None, C::Linear, kOnOff, "Matches output loudness to input so you can judge the effect fairly" },
        { "FxBoomSens",  "Punch Boom Sensitivity","Sens",0.f,100.f,50.f,U::Percent, C::Linear, {}, "How easily kicks trigger the sub (independent of track level)" },
    };
    jassert(d.size() == (size_t) Count);
    return d;
}

bool isChoice(int id) { return ! def(id).choices.empty(); }
bool isBool(int id)
{
    const auto& c = def(id).choices;
    return c.size() == 2 && juce::String(c[0]) == "Off" && juce::String(c[1]) == "On";
}

static juce::NormalisableRange<float> makeRange(const Def& d)
{
    const float lo = d.min, hi = d.max;
    switch (d.curve)
    {
        case C::Log:
            return { lo, hi,
                     [](float s, float e, float n) { return s * std::pow(e / s, n); },
                     [](float s, float e, float v) { return std::log(v / s) / std::log(e / s); },
                     [](float, float, float v) { return v; } };
        case C::Squared:
            return { lo, hi,
                     [](float s, float e, float n) { return s + (e - s) * n * n; },
                     [](float s, float e, float v) { return std::sqrt(juce::jlimit(0.f, 1.f, (v - s) / (e - s))); },
                     [](float, float, float v) { return v; } };
        case C::Linear:
        default:
            return { lo, hi, std::strcmp(d.id, "RetroDigiBits") == 0 ? 1.f : 0.f };
    }
}

juce::String formatValue(int id, float v)
{
    const auto& d = def(id);
    if (isChoice(id))
        return d.choices[(size_t) juce::jlimit(0, (int) d.choices.size() - 1, (int) std::lround(v))];
    switch (d.unit)
    {
        case U::dB:
            if (id == TapeHiss && v <= -89.95f) return "Off";
            return (v > 0.05f ? "+" : "") + juce::String(v, 1) + " dB";
        case U::Percent: return juce::String(juce::roundToInt(v)) + "%";
        case U::Hz:      return v >= 1000.f ? juce::String(v / 1000.f, v >= 10000.f ? 1 : 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz";
        case U::ms:      return juce::String(juce::roundToInt(v)) + " ms";
        case U::Bipolar: return (v > 0.5f ? "+" : "") + juce::String(juce::roundToInt(v));
        case U::None:
        default:         return juce::String(juce::roundToInt(v));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < Count; ++i)
    {
        const auto& d = def(i);
        const juce::ParameterID pid { d.id, 1 };
        if (isBool(i))
        {
            layout.add(std::make_unique<juce::AudioParameterBool>(pid, d.name, d.def >= 0.5f));
        }
        else if (isChoice(i))
        {
            juce::StringArray c;
            for (auto* s : d.choices) c.add(s);
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid, d.name, c, (int) d.def));
        }
        else
        {
            auto attrs = juce::AudioParameterFloatAttributes()
                             .withStringFromValueFunction([i](float v, int) { return formatValue(i, v); })
                             .withValueFromStringFunction([i](const juce::String& s) {
                                 auto t = s.trim().toLowerCase();
                                 if (i == TapeHiss && t == "off") return -90.f;
                                 float v = t.getFloatValue();
                                 if (def(i).unit == U::Hz && t.contains("k")) v *= 1000.f;
                                 return v;
                             });
            layout.add(std::make_unique<juce::AudioParameterFloat>(pid, d.name, makeRange(d), d.def, attrs));
        }
    }
    return layout;
}

Values defaultValues()
{
    Values v {};
    for (int i = 0; i < Count; ++i) v[(size_t) i] = def(i).def;
    return v;
}

int findByIdString(const juce::String& id)
{
    for (int i = 0; i < Count; ++i)
        if (id == def(i).id) return i;
    return -1;
}
} // namespace P
