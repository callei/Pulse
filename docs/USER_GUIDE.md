# Pulse

A drum processor for Windows (VST3 plugin + standalone app), rebuilt from the "Studio Drum Suite" prototype. Four modules in a row, in the order the sound flows through them:

**TAPE** (saturation & motion) → **GRIT** (lo-fi & texture) → **SHAPE** (transients) → **PUNCH** (weight & loudness)

## The finished files

| What | Where (after building) |
|---|---|
| VST3 plugin (for FL Studio and other DAWs) | `build\Pulse_artefacts\Release\VST3\Pulse.vst3` |
| Standalone app (runs without a DAW) | `build\Pulse_artefacts\Release\Standalone\Pulse.exe` |

`Pulse.vst3` is a folder: copy the whole folder, not the files inside it.

## Install in FL Studio

1. Copy `Pulse.vst3` into `C:\Program Files\Common Files\VST3\` (Windows will ask for admin permission, so click **Continue**). If an older Pulse is there, close FL Studio first and replace it.
2. Open FL Studio → **Options → Manage plugins**.
3. Click **Find installed plugins** (or **Start scan**). Pulse appears in the list under *PulseLab*. Click its star to make it a favourite if you like.
4. In the **Mixer**, click an empty insert slot on your drum track → **Select** → **Pulse** (or **More plugins…** if it isn't in the short list).

When you update Pulse later, close FL Studio, replace the file, and reopen. FL Studio picks up the new version; a rescan is only needed if Pulse doesn't show up.

**Latency**: Pulse delays the audio by about 7 ms and tells FL Studio so. FL Studio compensates for it automatically, so everything stays in time. You'd only notice it when monitoring a live input through the plugin.

## Using it

The window shows everything on one page. Every module has:
- a **power button** (top left of the card) to switch the whole module on or off,
- one or two **main knobs**,
- **More** (top right) to show every remaining control. **Less** hides them again.

**Knobs**: drag up/down · hold **Shift** while dragging for fine steps · **double-click** to reset · **mouse wheel** · **right-click** to type a value. Hover over anything to see what it does.

| Module | Main knobs | What it's for |
|---|---|---|
| **IN** | Input | Level into the chain (meter below) |
| **TAPE** | Drive, Bias | Tape saturation. Drive sets *how much*, and the level stays roughly constant. Formula and Speed change the character; More has wow/flutter, hiss, head bump and mix. |
| **GRIT** | Intensity, Flux | Lo-fi layers: Noise (vinyl/tape/VHS/hum), Wobble, Distort, Digital (bit/rate reduction), Magnetic (worn tape, dropouts). Switch layers on with the pills; More has their settings. Intensity scales all of them; Flux adds slow random movement. |
| **SHAPE** | Attack, Sustain | Transient shaper. Right = more snap / longer tail, left = softer / tighter. Pick Kick, Snare or Bus, and a character (Natural is the cleanest). The graph shows the gain it applies. Mojo adds a little colour. |
| **PUNCH** | Saturate, Push | **Boom** adds a tuned sub hit on every kick (the dot flashes when it fires). **Saturate** adds harmonics, and **Finish** makes it louder with a safe ceiling (never over −0.5 dB true peak). More has the Boom settings, the saturation mode and **Air** (high shelf). |
| **OUT** | Width, Mix, Output | Stereo width (0% mono … 200% extra wide), dry/processed mix, final level. **Match** keeps the output as loud as the input, so you can hear whether the processing is actually better, not just louder. |

**Top bar**
- **Presets**: `<` `>` step through them, or click the name for the list. Factory presets: Init, Tight Kick, Fat Snare, Bus Glue, Lo-Fi Loop, Tape Warmth, Subtle Polish. **Save** stores your own in `Documents\Pulse\Presets`. A `*` means you've changed something since loading.
- **A / B**: two settings side by side. B starts as a copy of A; **Copy** copies the current one to the other slot.
- **Undo / Redo**.
- **Quality**: *High* (default) is the cleanest; *Eco* uses about 40% less CPU and sounds nearly identical on most material.

Drag the bottom-right corner to resize the window (70% to 200%).

**CPU** (measured at 48 kHz, buffer 128): about 2.4% of one CPU core at High with default settings, 4.5% with every effect switched on. Eco: 1.5% / 2.7%.

## What changed from the original

The short version: correct timing (no more phasey mixes), no hidden clipping, much less aliasing, controls that actually work, no loudness jumps, no clicks, and a calmer interface. The details:
- `CHANGELOG.md` covers every difference and why, including the judgement calls.
- `COMPARISON.md` has measured side-by-side renders against the original.
- `DSP_SPEC.md` is the analysis of the original plugin and how Pulse is built.

## Rebuilding it yourself

See the build steps in the main [README](../README.md). Checks after building:

| Command | What it checks |
|---|---|
| `cd reference && node analyze_pulse.js` | Sound tests: timing, transparency, every bug of the original, extreme settings. Should end with `ALL PASS`. |
| `build/pulse_uishot_artefacts/Release/pulse_uishot.exe --selftest` | Presets, A/B, undo and saving/restoring a project. |
| [pluginval](https://github.com/Tracktion/pluginval) `--strictness-level 5 --validate build/Pulse_artefacts/Release/VST3/Pulse.vst3` | Industry-standard plugin validator. |

## Uninstalling

Delete `Pulse.vst3` from `C:\Program Files\Common Files\VST3\`. Your user presets are in `Documents\Pulse\Presets`.

## Folder map

| Folder | Contents |
|---|---|
| `src/` | The plugin: `dsp/` (the four modules), `engine/` (wiring, oversampling, latency), `ui/` (the interface) |
| `tests/` | Command-line tools for rendering audio and checking the interface |
| `reference/` | Test scripts and test signals |
| `docs/` | Spec, changelog, comparison |
