# Changelog: Pulse vs. the original "Studio Drum Suite VST"

Everything that is intentionally different from the original, and why. The original's behaviour is documented in `DSP_SPEC.md`.

## Naming
- Plugin renamed **Pulse**. Modules: **TAPE**, **GRIT** (was "Retro Color"), **SHAPE** (was the transient shaper), **PUNCH** (was the "Drum Butter"-style multi-FX). This avoids echoing commercial product names.
- Automation IDs keep the original parameter names (`TapeDrive`, `RetroNoiseAmt`, …), so the parameter list maps 1:1. Host-visible names use the new module names ("Grit Noise Amount").

## Whole plugin
- **Latency is reported to the host and is constant.** It's 136 samples (2.8 ms) at 48 kHz, made of the oversampling filters plus the tape's modulation headroom. It's identical in Eco and High, so switching quality never shifts your track. The original delayed audio by 5.2 ms without telling the host.
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
- **Level-compensated drive.** At small signals the tape stays at unity gain for any Drive, Formula or Bias. Drive now sets *how much saturation*, not how loud. The original added +11 dB at default settings.
- **DC blocker** after the biased (asymmetric) saturation stage.
- **Bias** now behaves like tape. Below 50%: more high-frequency drive into the saturator (brighter, grittier). Above 50%: darker playback. Asymmetry (even harmonics) still grows away from 50%.
- **Formula** sets both saturation steepness and pre/de-emphasis depth: Modern 3 dB, Vintage 4.5 dB, Aggressive 6 dB.
- **Speed and Formula changes glide**: filter frequencies and the curve move smoothly over 30 ms. No clicks.
- **Wow & flutter depth is in milliseconds** (wow up to ±0.8 ms, flutter up to ±0.25 ms), so it sounds the same at every sample rate. The original's depth halved at 96 kHz. Wow has a slow random drift, and flutter has a small random jitter. Interpolation is 4-point Hermite (was linear).
- **Hiss is shaped like tape hiss** (high-passed at 800 Hz, rolled off with the tape speed), and the dB value is now the actual RMS level. **Default changed from −70 dB to Off**, so several instances don't pile up noise.
- Tonal balance at default settings matches the original within ±1.5 dB per octave band once levels are matched.

## Parameter changes
| Parameter | Original | Pulse | Why |
|---|---|---|---|
| MasterWidth | 0–100% | 0–200% | Allow widening |
| TapeHiss | default −70 dB | default Off (−90) | No noise unless asked for |
| RetroWobbleMix | default 50% | default 100% | 100% = pure pitch wobble; the old default was a comb filter |
| *new* Quality | — | Eco / High | Oversampling choice |
| *new* AutoGain | — | Off / On | Loudness-matched A/B |
| *new* FxBoomSens | — | 0–100% | Level-independent Boom trigger |

Choice labels are shortened in the UI, e.g. "Modern Studio" → "Modern" and "Soft Tanh" → "Soft".
