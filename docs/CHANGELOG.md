# Changelog: Pulse vs. the original "Studio Drum Suite VST"

Everything that is intentionally different from the original, and why. The original's behaviour is documented in `DSP_SPEC.md`.

## Naming
- Plugin renamed **Pulse**. Modules: **TAPE**, **GRIT** (was "Retro Color"), **SHAPE** (was the transient shaper), **PUNCH** (was the "Drum Butter"-style multi-FX). This avoids echoing commercial product names.
- Automation IDs keep the original parameter names (`TapeDrive`, `RetroNoiseAmt`, …), so the parameter list maps 1:1. Host-visible names use the new module names ("Grit Noise Amount").

## Whole plugin
- **Latency is reported to the host and is constant**: 345 samples (7.2 ms) at 48 kHz (322 at 44.1 kHz, 621 at 96 kHz). FL Studio and every other modern DAW compensate it automatically, so tracks stay in time. It is the same in Eco and High and does not change when modules are switched on or off, so nothing ever shifts. It's made of: the oversampling filters, the tape's wow/flutter headroom (1.25 ms), the GRIT wobble headroom (2 ms), the SHAPE look-ahead (2 ms) and the true-peak limiter look-ahead (~0.8 ms). The original delayed audio by 5.2 ms (tape) plus 3.75 ms (wobble) without telling the host (DSP_SPEC issue 1).
  - *Judgement call*: a constant latency costs 7 ms even when SHAPE is set to a mode without look-ahead. Changing latency on the fly makes hosts re-align and can click, so constant was chosen.
- **The dry signal is time-aligned everywhere** (Master Mix, Tape Mix, module bypass). The original's mixes comb-filtered.
- **Oversampling** around the whole module chain: Eco = 2×, High = 4× (new `Quality` parameter, default High). It uses linear-phase filters, plus a linear-phase fractional delay that rounds the latency to a whole sample, so the bypass path is clean. Aliasing on a hard-driven 4.5 kHz tone: original −17 dB, Eco −28 dB, High −48 dB.
- **No hard clip at the output.** The original clamped every sample to ±1.0, even with all modules off. Pulse passes floating-point audio through untouched. The PUNCH finisher provides a true-peak safe ceiling when you want one.
- **Auto Gain** (new `AutoGain` parameter, "Match", default off): continuously matches output loudness to input loudness (1 s integration, ±18 dB range, holds during silence), so you can judge the processing fairly.
- **Width range is now 0–200%** (was 0–100%, which could only narrow).
- **Master Mix takes its dry signal after Input Gain** (it was before), so Input Gain works as a trim for the whole plugin and parallel blending stays level-consistent.
- Every continuous parameter is smoothed (30 ms ramps). Choice and on/off changes glide or crossfade instead of jumping.
- Random modulation (wow drift, flutter jitter, hiss, and later crackle, dropouts and flux) is **deterministic**. It reseeds on every playback start or reset from a per-instance seed stored in the project, so offline bounces are repeatable.
- Denormal protection on the audio thread (`ScopedNoDenormals`).
- Works mono and stereo, at any sample rate (tested at 44.1/48/96 kHz) and any block size (output is bit-identical at block size 1, 37 and 512).

## TAPE
- **Level-compensated drive.** The tape is level-matched for drum-level material (calibrated at a −12 dBFS peak) for any Drive, Formula or Bias. On a drum loop, loudness stays within +0.8…+3.6 dB over the whole −24…+24 dB drive range: it gets a little louder as it gets denser, as real saturation does. Drive now sets *how much saturation*, not how loud. The original added +10 dB at default settings and +14.5 dB at +18 dB drive.
  - *Judgement call*: an earlier version calibrated at very quiet levels (exact unity for quiet signals), but hot settings then came out up to 12.8 dB *quieter*. Quiet passages are now ~1.6 dB louder at default drive instead.
- **DC blocker** after the biased (asymmetric) saturation stage.
- **Bias** now behaves like tape. Below 50%: more high-frequency drive into the saturator (brighter, grittier). Above 50%: darker playback. Asymmetry (even harmonics) still grows away from 50%.
- **Formula** sets both saturation steepness and pre/de-emphasis depth: Modern 3 dB, Vintage 4.5 dB, Aggressive 6 dB.
- **Speed and Formula changes glide**: filter frequencies and the curve move smoothly over 30 ms. No clicks.
- **Wow & flutter depth is in milliseconds** (wow up to ±0.8 ms, flutter up to ±0.25 ms), so it sounds the same at every sample rate. The original's depth halved at 96 kHz. Wow has a slow random drift, and flutter has a small random jitter. Interpolation is 4-point Hermite (was linear).
- **Hiss is shaped like tape hiss** (high-passed at 800 Hz, rolled off with the tape speed), and the dB value is now the actual RMS level. **Default changed from −70 dB to Off**, so several instances don't pile up noise.
- Tonal balance at default settings matches the original within ±1.5 dB per octave band once levels are matched.

## GRIT (was "Retro Color")
- **Every noise type is distinct** (the original's VHS was identical to Tape, issue 8). Vinyl: random crackle (about 14 clicks/s) plus occasional pops and surface noise. Tape: shaped hiss. VHS: 59.94 Hz buzz, band-limited noise and a faint 15.7 kHz line whine. Hum: 60 Hz with harmonics. All four are calibrated to the same RMS level at the same Amount (within 2 dB).
- **Hum works without TAPE** (it became DC when Tape was bypassed, issue 9).
- **Noise Tone and Magnetic Wear now do something** (both were dead, issue 7). Tone tilts the noise darker/brighter; Wear dulls the highs (20 kHz → 4 kHz) and adds soft compression and a slight level wobble.
- **Flux is a slow random movement** (two smooth random LFOs at ~0.6 and ~1.9 Hz) of noise level, wobble depth/rate and distortion drive, as the name promised. It was a ~38 Hz jitter on noise level only (issue 16).
- **Wobble is time-aligned and its depth is in milliseconds** (up to 1.6 ms), so it's the same at every sample rate. Wobble Mix at 100% (new default) is pure pitch wobble; below 100% the dry part is aligned, so it's a chorus rather than a comb filter (issue 2).
- **Distortion is oversampled and bounded.** Aliasing on a fuzzed tone: original −17 dB, Eco −24 dB, High −33 dB. Transformer was unbounded (+37 dB internally, issue 10) and is now a bounded curve. Every type is level-matched at a −12 dBFS band level, and Focus still picks the band that gets distorted.
- **Digital Smooth is a real low-pass** after the reducer. The sample-rate reducer goes from the full rate down to 1.5 kHz.
- **Dropouts are click-free**: random level dips (−6 to −20 dB, 30–200 ms) with 8 ms fades. The original produced 67 clicks in 2 s (issue 11).
- *Judgement call*: the order inside GRIT is Noise → Wobble → Distort → Digital → Magnetic, like the original, so noise also gets wobbled and degraded.

## SHAPE (transient shaper)
- **Level-independent detection.** Attack/Sustain react to the *shape* of the sound (fast vs. slow envelope in dB), not its level. The same setting does the same thing on a quiet or a loud kick (issue 12).
- **Sane range**: Attack up to ±15 dB, Sustain up to ±12 dB (the original reached +30–42 dB).
- **Stereo-linked**: both channels get the same gain, so the stereo image doesn't wander.
- **Natural** character uses a real 2 ms look-ahead, so it catches the very start of a hit. Classic and Smooth react without look-ahead, as before.
- Below −75 dBFS the shaper does nothing, so it never pumps up noise floors or reverb tails.
- **Mojo** Type changes crossfade (no clicks), and the character stage is level-matched.
- A live **gain trace** shows what the shaper is doing.

## PUNCH (was the multi-FX)
- **Boom triggers on kicks relative to the running low end**, with a new **Sensitivity** control. It fires the same way whether the track is at 0 dB or −18 dB; the original used a fixed level and missed kicks 12 dB down (issue 13). Each hit resets the sub's phase and adds a quick pitch drop, so every boom is identical and click-free. Two voices overlap smoothly on fast hits. 100% Level gives a sub peaking around −1.4 dBFS.
- **Saturate** modes are bounded and level-matched at −6 dBFS peaks: Console, Tape, Digital, Tube (with DC blocker, issue 14) and Bloom (adds a gentle low lift). They blend in over the first 20% of Amount, so small settings are subtle.
- **Air works without Finish** (it only worked with "More" on, issue 7): a smooth 10 kHz shelf, −6…+12 dB.
- **Finish is true-peak safe.** It pushes up to +10 dB into a soft-knee ceiling, then a true-peak look-ahead limiter (8× oversampled peak detection) holds the output at −0.5 dBTP. The original reached +3.2 dBTP on a hot loop (issue 5). Note: Output Gain, Width and Mix come *after* Finish, so turning Output up can still exceed the ceiling. That's deliberate, since they're your final trims.

## Interface
- **One page**, in signal order: IN (input gain + meter) → TAPE → GRIT → SHAPE → PUNCH → OUT (width, mix, Match, output gain + meter). The original's tabs, scope and decorative graphics are gone.
- Each module card has a **power button**, one or two **main knobs**, the main selectors, and a **More** button that reveals every remaining control, grouped by section with each section's on/off switch. Controls of a switched-off section are greyed out.
- **Knobs**: drag up/down, **Shift**-drag for fine steps, **double-click** to reset, **mouse wheel**, **right-click to type** a value. Hovering shows the value, and every control has a tooltip.
- **Readouts**: input/output peak meters with peak hold, SHAPE gain trace, a Boom LED that flashes on every trigger, and the current Match correction in dB.
- **Resizable** from 70% to 200%, always at the same proportions. Each instance remembers its size in the project.
- Undo/Redo covers knob moves, preset loads and A/B switches. *Note*: host automation playback also lands in the undo history; that's JUCE's standard behaviour.

## Presets
- Factory: **Init, Tight Kick, Fat Snare, Bus Glue, Lo-Fi Loop, Tape Warmth, Subtle Polish**. Use the previous/next buttons, or click the name for the list. A `*` after the name means you've changed something since loading.
- **User presets** are saved as files in `Documents\Pulse\Presets` (one `.pulsepreset` file each, easy to back up or share).
- **A/B**: two complete settings to compare. B starts as a copy of A, and **Copy** copies the current one to the other slot.
- *Judgement call*: presets don't change **Quality** or **Match**. Those are workflow settings, so browsing presets never changes your CPU load or level matching. A/B slots live while the plugin is open; they aren't saved in the project.

## Parameter changes
| Parameter | Original | Pulse | Why |
|---|---|---|---|
| MasterWidth | 0–100% | 0–200% | Allow widening |
| TapeHiss | default −70 dB | default Off (−90) | No noise unless asked for |
| RetroWobbleMix | default 50% | default 100% | 100% = pure pitch wobble; the old default was a comb filter |
| *new* Quality | — | Eco / High | Oversampling choice |
| *new* AutoGain | — | Off / On | Loudness-matched A/B |
| *new* FxBoomSens | — | 0–100% | Level-independent Boom trigger |
| TransAttack / TransSustain | up to ~+42 dB | ±15 dB / ±12 dB | Usable range |
| FxAir | only with "More" on | always active | Was a dead control |

Choice labels are shortened in the UI, e.g. "Modern Studio" → "Modern" and "Soft Tanh" → "Soft".
