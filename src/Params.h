#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// Single source of truth for all parameters.
// IDs 0..55 keep the original plugin's parameter names (= host automation IDs) and order.
// Host-visible names use the new module names: TAPE, GRIT (was Retro), SHAPE (was Trans), PUNCH (was Fx).
namespace P
{
enum Id : int
{
    InputGain, OutputGain, MasterMix, MasterWidth,
    TapeBypass, TapeDrive, TapeSpeed, TapeBias, TapeFormula, TapeWow, TapeFlutter, TapeHiss, TapeHeadBump, TapeMix,
    RetroBypass, RetroMagnitude,
    RetroNoiseOn, RetroNoiseAmt, RetroNoiseType, RetroNoiseTone, RetroNoiseFlux,
    RetroWobbleOn, RetroWobbleAmt, RetroWobbleStereo, RetroWobbleMix,
    RetroDistOn, RetroDistAmt, RetroDistType, RetroDistFocus,
    RetroDigiOn, RetroDigiAmt, RetroDigiBits, RetroDigiSmooth,
    RetroMagOn, RetroMagAmt, RetroMagDropouts,
    TransBypass, TransMode, TransAlgo, TransAttack, TransSustain, TransMojoOn, TransMojoAmt, TransMojoType, TransMix,
    FxBypass, FxBoomOn, FxBoomAmt, FxBoomPitch, FxBoomDecay, FxSatOn, FxSatAmt, FxSatMode, FxMoreOn, FxMoreAmt, FxAir,
    // ---- new in Pulse ----
    Quality, AutoGain, FxBoomSens,
    Count
};

enum class Unit { None, dB, Percent, Hz, ms, Bipolar };
enum class Curve { Linear, Log, Squared };

struct Def
{
    const char* id;      // automation ID (stable)
    const char* name;    // host-visible name
    const char* label;   // short UI label
    float min, max, def;
    Unit unit;
    Curve curve;
    std::vector<const char*> choices; // non-empty => choice param; {"Off","On"} => bool param
    const char* tooltip;
};

const std::vector<Def>& defs();
inline const Def& def(int id) { return defs()[(size_t) id]; }
bool isBool(int id);
bool isChoice(int id);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
juce::String formatValue(int id, float plain);

// Plain-value snapshot passed to the DSP once per block.
using Values = std::array<float, Count>;
Values defaultValues();
int findByIdString(const juce::String& id); // -1 if unknown
} // namespace P
