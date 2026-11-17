# DSP spec of the original "Studio Drum Suite VST" (v8, `.fmx`)

This document describes what the original WebAssembly DSP (`extracted/dsp.wasm`, AssemblyScript, ABI v1) actually does. It was recovered by decompiling with WABT 1.0.41 (`wasm-decompile` → `analysis/dsp.dcmp`), checking ambiguous expressions against the exact WAT, and confirming behaviour by rendering through the original binary (`reference/render.js`, `reference/analyze_original.js`; raw results in `reference/out/analysis_report.txt`).

All numbers below are the binary's real constants. "SR" = sample rate.

## 1. Host ABI

| Item | Value |
|---|---|
| Exports | `init(sr: f32)`, `process(inL, inR, outL, outR: i32 ptr, nFrames: i32)`, `reset()`, `getNumParams() → 56`, `getAbiVersion() → 1`, `memory` |
| Start function | Allocates all state (biquads, four 4096-sample delay lines, telemetry buffers) via a bump allocator |
| Parameters | 56 × f32 at memory byte offset **65536 + 4·id**, in **plain units** (dB, %, Hz, ms, choice index). Booleans test `≥ 0.5`. |
| Param smoothing | Each continuous param has a one-pole smoothed copy at 65792 + 4·id. Coefficient `exp(-1000 / (SR · 5 ms))`. TapeHiss uses 50 ms. Choice params and on/off switches are **not** smoothed. |
| Block size | ≤ 4096 frames (indices wrap at 4096) |
| Telemetry (`hostExposeBuffer`) | `meters[4]` = peak in L, in R, out L, out R per block (input measured *before* InputGain). `scope[64]` = (L+R)/2 of every 8th output sample, ring. `transient[4]` = fast env avg, slow env avg, transient gain dB avg, boom-active flag. |
| `init` | Stores SR, computes the smoothing coefficients, snaps smoothed params to their targets, clears delay lines, designs the fixed filters. Params must be written **before** `init`. |
| `reset` | Clears delay lines and transient/boom state. Does not clear biquad states. |
| Random source | One shared xorshift32 (seed 1592594996), uniform in [−1, 1]. Deterministic per instance. |

Filters are RBJ-cookbook biquads (transposed direct form II), with their coefficients recomputed at the start of every `process` call for anything that depends on a parameter.

Bypass semantics: `TapeBypass`, `RetroBypass`, `TransBypass` and `FxBypass` = 1 means **bypassed**. Each module is hard-switched in or out with no crossfade.

## 2. Signal flow (per sample, stereo L/R)

```
in ──► InputGain ──► [1 TAPE] ──► [2 RETRO: Noise → Wobble → Distort → Digital → Magnetic] ──►
       [3 TRANSIENT (+MOJO)] ──► [4 FX: Boom → Saturate → (Air + More)] ──► Width (M/S) ──►
       MasterMix (vs. raw input) ──► OutputGain ──► hard clip ±1.0 ──► out
```

### Global

| Param | Implementation |
|---|---|
| InputGain (id 0) | `x · 10^(dB/20)`, smoothed |
| MasterWidth (id 3) | `M = (L+R)/2`, `S = (L−R)/2 · width%`. Range 0–100%, so it **can only narrow**. |
| MasterMix (id 2) | Linear crossfade between the processed signal and the **raw input (before InputGain)**. The dry path is **not delay-compensated**. |
| OutputGain (id 1) | `· 10^(dB/20)`, then **hard clamp to ±1.0**. This is always active, even with every module bypassed. |

### 2.1 Tape (ids 4–13)

Order inside the module:

1. **Drive**: `x · 10^(Drive/20)`.
2. **Pre-emphasis**: high shelf at 6 kHz, **+3.5 dB**, S = 1 (Q ≈ 0.707).
3. **Bias offset**: `b = (Bias% / 100 − 0.5) · 0.15`, so in the range ±0.075. It is added before the shaper.
4. **Shaper**: `y = clamp(P(k·(x + b)), ±1) − 0.7·b`, where `P(u) = u(27 + u²)/(27 + 9u²)` (a Padé tanh approximation). Formula sets `k`:
   - Modern Studio: k = 1.8
   - Vintage Warm: k = 2.4
   - Aggressive: k = 3.2

   The DC offset is only partly removed (−0.7·b), with no DC blocker. The shaper has **no oversampling** and **no output-gain compensation**: small-signal gain at default settings is **+11.1 dB**.
5. **De-emphasis**: high shelf at 6 kHz, **−3.5 dB**.
6. **Head bump**: peaking EQ, Q = 1.5, **+2.5 dB** when HeadBump is on (0 dB when off). Frequency depends on Speed (see table below).
7. **HF loss**: 2nd-order Butterworth low-pass. Cutoff depends on Speed (see table below).

   | Speed | Head bump | Low-pass |
   |---|---|---|
   | 7.5 ips | 110 Hz | 12 kHz |
   | 15 ips | 75 Hz | 16 kHz |
   | 30 ips | 55 Hz | 20 kHz |
   | Cassette | 95 Hz | 9.5 kHz |

8. **Wow & flutter**: modulated delay line, linear interpolation. Delay in **samples** = `250 + sin(φw)·Wow%·0.35 + sin(φf)·Flutter%·0.12`.
   - Wow LFO: L 0.8 Hz, R 0.85 Hz.
   - Flutter LFO: L 12 Hz, R 13.5 Hz.
   - Maximum depth is ±35 samples of wow and ±12 of flutter.
   - Because the delay is in samples, the depth halves at 96 kHz.
   - The **250-sample base delay (5.2 ms at 48 kHz) is always present and never reported as latency.**
9. **Hiss**: if Hiss > −88 dB, add white noise `uniform · 10^(Hiss/20) · 0.4`. It is unfiltered, flat to Nyquist, and independent per channel.
10. **Mix**: crossfade against the module's dry input. Because the wet path is delayed by 250 samples, **Mix < 100% comb-filters**. Measured at 50% mix: notches at 96, 288, 480 Hz… with a 2 dB ripple at −24 dB drive. The notches get deeper the closer the wet and dry levels are.

### 2.2 Retro colour chain (ids 14–35)

`RetroMagnitude` (`mag`, 0–1) scales every sub-stage's amount. There is no module-level mix.

**Noise** (on 16, amt 17, type 18, tone 19, flux 20)
- **Flux**: two smoothed random signals, `r += (uniform − r)·0.005` per sample. That is a one-pole low-pass on white noise at ~38 Hz (48 kHz), so a jittery random signal rather than a slow LFO. Only the first is used. It modulates **only the noise level**: `level · (1 + r·Flux%·0.5)`. It does not modulate wobble or drive, despite what the UI text claims.
- Noise types:
  - **Vinyl Crackle**: `click = (|u| > 0.985) ? u·2.5 : 0`. That is a ~1.5% chance of a click per sample (~720 clicks/s at 48 kHz, closer to fizz than crackle). The same click goes to both channels, plus white noise × 0.15 per channel.
  - **Tape Hiss**: white noise.
  - **VHS Buzz**: white noise, **byte-identical to Tape Hiss** (confirmed by a null test).
  - **Circuit Hum**: `sin(75·φwow)·0.4` + white × 0.1. That is 60 Hz, derived from the *Tape* module's wow LFO phase (0.8 Hz × 75). **When Tape is bypassed the phase is frozen, so the hum becomes a DC offset** (confirmed).
- Output: `noise · Amt% · mag · 0.08 · fluxMod`, added to the signal.
- **Tone (id 19) is never read** (null test: identical output).

**Wobble** (on 21, amt 22, stereo 23, mix 24)
- Modulated delay. In samples: `180 + sin(φ)·Amt%·mag·80`.
- LFO frequencies: L 1.4 Hz; R 1.7 Hz with a +90° offset when Stereo is on, otherwise 1.4 Hz.
- Mix crossfades against the dry signal. With a 180-sample base delay, the **default 50% mix is a static comb filter / flanger** (first notch 133 Hz at 48 kHz) rather than a pitch wobble. The delay is sample-rate dependent.

**Distort** (on 25, amt 26, type 27, focus 28)
- Band-pass (constant 0 dB peak, Q = 1) at Focus (clamped 100 Hz–16 kHz). The band is removed from the signal, driven by `1 + Amt%·mag·8`, shaped, and added back.
- Shapers:
  - **Soft Tanh**: `clamp(P(x), ±1)`.
  - **Tube Asym**: like Soft Tanh but scaled ×1.3 above zero and ×0.7 below. Asymmetric, with no DC blocker.
  - **Diode Clip**: `clamp(x, −0.65, 0.45) · 1.4`. Asymmetric hard clip.
  - **Hard Fuzz**: `clamp(2x, ±0.85)`.
  - **Transformer**: `x − 0.22x³`. **Unbounded**: at full drive a −20 dBFS input reaches about +37 dBFS internally, then folds over and gets hard-clipped at the output.
- No oversampling. Measured aliasing/inharmonic energy is **−17 dB** relative to the harmonics (Hard Fuzz, 4.5 kHz sine).

**Digital** (on 29, amt 30, bits 31, smooth 32)
- Sample-and-hold downsampling, holding for `1 + Amt%·mag·18` samples (non-integer hold lengths are counted with a float counter).
- At each new sample: `round(clamp(x, ±1) · 2^Bits) / 2^Bits`.
- **Smooth** is not a filter. It blends back up to 30% of the un-crushed signal: `y = held + (x − held)·Smooth%·0.3`.

**Magnetic** (on 33, amt 34, dropouts 35)
- When not in a dropout: each sample has a chance `Dropouts%·mag·0.003` of starting one. A dropout lasts 40 ms at gain 0.15.
- The gain **switches instantly** between 1 and 0.15. Measured: 67 audible discontinuities in 2 s at 100%.
- **Amount (id 34) is never read** (null test: identical output).

### 2.3 Transient shaper (ids 36–44)

- **Detector**: band-pass (Q = 0.707) per channel. Stereo is **unlinked**: each channel gets its own gain, so the stereo image shifts.

  | Mode | BP freq | Fast att / rel | Slow att / rel |
  |---|---|---|---|
  | Kick | 90 Hz | 1 / 50 ms | 25 / 250 ms |
  | Snare | 2200 Hz | 0.3 / 25 ms | 15 / 150 ms |
  | Bus | 1000 Hz | 0.5 / 30 ms | 20 / 180 ms |

  Envelope coefficient = `exp(−1000/(SR·ms))`, peak followers on |bp|.
- **Differential**:
  - `atk = clamp(max(fast − slow, 0)/(slow + 1e-4), 0, 3)`
  - `sus = clamp(max(slow − fast, 0)/(slow + 1e-4), 0, 2)`

  These are ratios, so mostly level-independent, except the 1e-4 floor, which mutes detection below about −80 dBFS.
- **Gain (dB)** = `Attack/100 · atk · 14 + Sustain/100 · sus · 10`. Maximum is **+42 dB** of attack boost and ±20 dB of sustain. Measured: +30 dB on a −30 dBFS kick at Attack +100. That is far beyond musical range.
- **Gain smoothing (Algo)**: one-pole with a **per-sample** coefficient (not scaled to SR): Classic 0.99, Natural 0.996, Smooth 0.998. That is roughly 2 / 5 / 10 ms at 48 kHz, and half that at 96 kHz. "Natural" has **no look-ahead**.
- **MOJO**: filter, then `0.85·clamp(P(1.3·x), ±1)`, crossfaded with the signal by MojoAmt.

  | Mojo type | Filter |
  |---|---|
  | Punch / Crack | Peaking 3.5 kHz, Q 1.2, +4 dB |
  | Sub / Body | Low shelf 120 Hz, +3.5 dB |
  | Air / De-Harsh | High shelf 8 kHz, −3 dB |

  Not oversampled.
- **Mix**: crossfade against the module input.

### 2.4 Drum multi-FX (ids 45–55)

**Boom** (on 46, amt 47, pitch 48, decay 49)
- **Trigger**: 120 Hz Butterworth LP on L and R. Fires if `|L+R|/2 > 0.28`. That is a **fixed absolute threshold (≈ −11 dBFS of low end)**: measured, a kick peaking at −6 dBFS triggers it, but the same kick 12 dB quieter never does.
- After a trigger there is an 80 ms hold-off. The trigger filters stop updating during the hold-off, so their state goes stale.
- **Voice**: sine. Frequency = `Pitch · (1 + 1.8·pitchEnv)`, with pitchEnv decaying with τ = 25 ms (a 2.8× pitch drop). Amplitude envelope = 1 at the trigger, decaying with τ = Decay ms.
- Output: `clamp(P(1.6·sin·env), ±1) · Amt% · 0.7`, added to both channels.
- **The oscillator phase is not reset on trigger**, so every hit starts at a random phase and attack shape, and some hits click.

**Saturate** (on 50, amt 51, mode 52)
- Drive `1 + 4·Amt%`. Output is crossfaded with the dry signal by the same Amt%, with no gain compensation.
- Modes:
  - **Console**: `x − 0.15x³`. **Unbounded**: folds over above |x| ≈ 1.5, which is easy to reach with drive up to 5×.
  - **Tape**: `P(1.2x)`.
  - **Digital**: `clamp(x, ±0.7)·1.35`.
  - **Tube**: `x/(1+x)` for x > 0, `x/(1−0.5x)` for x < 0. Asymmetric, no DC blocker.
  - **Flub Bloom**: `1.1·P(x)`.
- No oversampling. Measured aliasing is −24 dB relative to the harmonics (Digital mode).

**More** (on 53, amt 54) and **Air** (55)
- **Air** is a high shelf at 10 kHz, S = 1, gain = FxAir dB. It is computed **inside the More block**, so **Air has no effect when More is off** (null test confirmed).
- **More**: `y = P(x·d)/(0.85·d)` with `d = 1 + 2·Amt%`, clamped to ±1. Small-signal gain is +1.4 dB at every Amount, and the ceiling falls as Amount rises. Not oversampled.

## 3. Measured issues summary

| # | Issue | Evidence |
|---|---|---|
| 1 | Unreported latency: Tape always delays by 250 samples, Wobble by 180 | Impulse test: 251 / 180 samples |
| 2 | Comb filtering with Tape Mix < 100%, Wobble Mix (default 50%), and MasterMix < 100% (undelayed dry) | Level ripple at 96/192/288 Hz |
| 3 | Wow/flutter/wobble depth and transient smoothing depend on sample rate | Tape delay 5.23 ms at 48 kHz vs 2.61 ms at 96 kHz |
| 4 | No oversampling in any nonlinear stage | Alias energy −17 dB (tape, fuzz), −24 dB (digital sat) |
| 5 | Hard clip at 0 dBFS on the output, always on; inter-sample overs | +6 dBFS in with everything bypassed gives a clipped output; +3.16 dBTP measured |
| 6 | Big loudness jumps: Tape +11 dB at defaults, loop +8.6 dB RMS at defaults | Measured |
| 7 | Dead params: Noise Tone, Magnetic Amount; Air only works with More on | Null tests |
| 8 | Duplicate option: VHS Buzz = Tape Hiss | Null test |
| 9 | Hum depends on the Tape module running | Hum becomes DC when Tape is bypassed |
| 10 | Unbounded shapers: Transformer, Console | +37 dB internal level |
| 11 | Clicks: dropouts, boom phase, hard bypass, unsmoothed choice switches | 67 clicks in 2 s |
| 12 | Transient shaper range up to +42 dB; unlinked stereo; no look-ahead | Measured +30 dB |
| 13 | Boom trigger uses a fixed absolute threshold | No trigger 12 dB down |
| 14 | No DC blocking after asymmetric stages (tape bias, tube, diode) | Code |
| 15 | Denormals: there is no flush-to-zero anywhere, and the decaying filters, envelopes and delay lines all generate subnormals on silence. WebAssembly has no FTZ mode, so a native port needs FTZ/DAZ or it will get CPU spikes. | Code |
| 16 | Flux is a ~38 Hz jitter on noise level only, not a "smooth random LFO" for drive/noise/wobble | Code |

Good things worth keeping: the overall chain idea; pre/de-emphasis around the tape shaper (HF saturates first); speed-dependent head bump and HF loss; band-focused distortion (only the Focus band gets distorted); the dual-envelope ratio transient detection; per-mode detector tuning; the Boom pitch-drop envelope; the cheap overall CPU (wasm ≈ 177× real-time in Node).
