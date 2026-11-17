// Measures Pulse (via build/.../pulse_render.exe) with the same kind of tests used on the original.
// Usage: node analyze_pulse.js            -> prints PASS/FAIL lines and measurements
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const { readWav, writeWav } = require('./wav');
const { SR, db, peak, rms, maxDiff, sine, fftMag, aliasRatio, truePeak } = require('./metrics');

const EXE = path.join(__dirname, '..', 'build', 'pulse_render_artefacts', 'Release', 'pulse_render.exe');
const TMP = fs.mkdtempSync(path.join(os.tmpdir(), 'pulse-'));
const ALL_OFF = { TapeBypass: 1, RetroBypass: 1, TransBypass: 1, FxBypass: 1 };
let tmpN = 0, failures = 0;

function render(sig, values = {}, { sr = SR, trim = true, block = 512, stereo = null } = {}) {
  const inF = path.join(TMP, `in${tmpN}.wav`), outF = path.join(TMP, `out${tmpN++}.wav`);
  writeWav(inF, sr, stereo || [sig, sig]);
  const args = [inF, outF, '--block', String(block), ...Object.entries(values).map(([k, v]) => `${k}=${v}`)];
  if (trim) args.push('--trim');
  execFileSync(EXE, args);
  return readWav(outF).channels;
}
function latencyOf(values, sr) { return +execFileSync(EXE, [writeTmpSilence(sr), path.join(TMP, 'lat.wav'), '--info', ...Object.entries(values).map(([k, v]) => `${k}=${v}`)]).toString().match(/latency (\d+)/)[1]; }
function writeTmpSilence(sr) { const f = path.join(TMP, `sil${sr}.wav`); if (!fs.existsSync(f)) writeWav(f, sr, [new Float32Array(sr / 10), new Float32Array(sr / 10)]); return f; }
const argmax = (x) => { let m = 0; x.forEach((v, i) => { if (Math.abs(v) > Math.abs(x[m])) m = i; }); return m; };
const gainDb = (out, inp) => db(rms(out.subarray(out.length / 2)) / rms(inp.subarray(inp.length / 2)));
function log(name, value, ok = null) {
  const tag = ok === null ? '     ' : ok ? 'PASS ' : 'FAIL ';
  if (ok === false) failures++;
  console.log(`${tag}${name.padEnd(60)} ${value}`);
}

const loop = readWav(path.join(__dirname, 'signals', 'loop.wav')).channels;

// ---- Latency & transparency -------------------------------------------------------------------
for (const sr of [44100, 48000, 96000]) for (const q of [0, 1]) {
  const lat = latencyOf({ Quality: q }, sr);
  const imp = new Float32Array(sr / 2); imp[1000] = 0.5;
  const [l] = render(imp, { ...ALL_OFF, Quality: q }, { sr, trim: false });
  const m = argmax(l) - 1000;
  log(`latency @${sr} ${q ? 'High' : 'Eco '}: reported ${lat}, measured`, `${m}`, m === lat);
}
{ const out = render(null, ALL_OFF, { stereo: loop });
  log('all bypassed: max sample error vs input (loop)', `${db(maxDiff(out[0], loop[0])).toFixed(1)} dB (band-limit >20 kHz only)`, db(maxDiff(out[0], loop[0])) < -20); }

// ---- Tape ---------------------------------------------------------------------------------------
const TAPE = { ...ALL_OFF, TapeBypass: 0, TapeHiss: -90 };
{ const s = sine(1000, 1, -30); const [l] = render(s, TAPE); const g = gainDb(l, s);
  log('tape small-signal gain at default drive (orig +11.1 dB)', `${g.toFixed(2)} dB`, Math.abs(g) < 1); }
for (const mix of [50]) {
  const r = [96, 192, 288].map((f) => { const s = sine(f, 1, -30); const [l] = render(s, { ...TAPE, TapeDrive: -24, TapeHeadBump: 0, TapeWow: 0, TapeFlutter: 0, TapeMix: mix }); return gainDb(l, s); });
  const ripple = Math.max(...r) - Math.min(...r);
  log(`tape mix ${mix}%: level at 96/192/288 Hz (orig ripple 2 dB)`, r.map((x) => x.toFixed(2)).join(' / '), ripple < 0.3);
}
{ const r = [96, 192, 288, 2000].map((f) => { const s = sine(f, 1, -30); const [l] = render(s, { ...TAPE, TapeDrive: -24, TapeHeadBump: 0, TapeWow: 0, TapeFlutter: 0, MasterMix: 50 }); return gainDb(l, s); });
  log('master mix 50% with tape: level at 96/192/288/2k Hz', r.map((x) => x.toFixed(2)).join(' / '), Math.max(...r) - Math.min(...r) < 0.5); }
const s45 = sine(4500, 1, -6);
for (const q of [0, 1]) { const [l] = render(s45, { ...TAPE, TapeDrive: 18, TapeFormula: 2, TapeWow: 0, TapeFlutter: 0, Quality: q });
  const a = aliasRatio(l.subarray(8192), 4500); log(`tape alias energy, drive +18 Aggressive, ${q ? 'High' : 'Eco'} (orig -17.1)`, `${a.toFixed(1)} dB`, a < (q ? -40 : -25)); }
{ const z = new Float32Array(SR * 2); const [l] = render(z, { ...TAPE, TapeHiss: -60 });
  const r = db(rms(l.subarray(SR))); log('tape hiss -60 dB on silence: output RMS', `${r.toFixed(1)} dB`, Math.abs(r + 60) < 2); }

// ---- Robustness ---------------------------------------------------------------------------------
{ const maxAll = {}; const { PARAMS } = require('./render');
  for (const p of PARAMS) maxAll[p.name] = p.max;
  Object.assign(maxAll, { TapeBypass: 0, RetroBypass: 0, TransBypass: 0, FxBypass: 0 });
  let ok = true; try { const out = render(null, maxAll, { stereo: loop.map((c) => c.map((v) => v * 4)) }); ok = out.every((c) => c.every(Number.isFinite)); } catch { ok = false; }
  log('all params at max, +12 dB hot input: finite output', ok ? 'yes' : 'NO', ok); }
{ const a = render(null, {}, { stereo: loop, block: 512 }), b = render(null, {}, { stereo: loop, block: 1 }), c = render(null, {}, { stereo: loop, block: 37 });
  const d1 = maxDiff(a[0], b[0]), d2 = maxDiff(a[0], c[0]);
  log('block size 512 vs 1 vs 37: max difference', `${db(d1).toFixed(1)} / ${db(d2).toFixed(1)} dB`, d1 < 1e-4 && d2 < 1e-4); }
{ const a = render(null, { TapeWow: 100, TapeFlutter: 100, TapeHiss: -50 }, { stereo: loop }), b = render(null, { TapeWow: 100, TapeFlutter: 100, TapeHiss: -50 }, { stereo: loop });
  log('repeat render with random modulation: identical (deterministic)', maxDiff(a[0], b[0]) === 0 ? 'yes' : 'NO', maxDiff(a[0], b[0]) === 0); }
{ const mono = render(null, {}, { stereo: [loop[0]] }); log('mono input: finite output', mono.length === 1 && mono[0].every(Number.isFinite) ? 'yes' : 'NO', mono.length === 1); }
{ const t = Date.now(); render(null, { Quality: 1 }, { stereo: [loop[0].subarray(0), loop[1]] }); const dt = (Date.now() - t) / 1000;
  log('speed incl. process start (High, defaults, 4.2 s loop)', `${(4.2 / dt).toFixed(0)}x realtime`); }

fs.rmSync(TMP, { recursive: true, force: true });
console.log(failures ? `\n${failures} FAILURE(S)` : '\nALL PASS');
process.exit(failures ? 1 : 0);
