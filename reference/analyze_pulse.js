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
  log('tape quiet-signal gain at default drive (orig +11.1 dB)', `${g.toFixed(2)} dB`, Math.abs(g) < 2); }
{ const lvl = (d) => { const out = render(null, { ...TAPE, TapeWow: 0, TapeFlutter: 0, TapeDrive: d, TapeFormula: 2 }, { stereo: loop }); return db(rms(out[0])) - db(rms(loop[0])); };
  const v = [-24, 0, 12, 24].map(lvl);
  log('tape loop loudness, drive -24/0/+12/+24 (orig +10.0 at +6, +14.5 at +18)', v.map((x) => x.toFixed(1)).join(' / ') + ' dB', v.every((x) => x > -1 && x < 4.5)); }
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

// ---- GRIT ---------------------------------------------------------------------------------------
const GRIT = { ...ALL_OFF, RetroBypass: 0 };
const differs = (a, b) => maxDiff(a, b) > 1e-4;
{ const a = render(null, { ...GRIT, RetroNoiseOn: 1, RetroNoiseTone: -10 }, { stereo: loop })[0], b = render(null, { ...GRIT, RetroNoiseOn: 1, RetroNoiseTone: 10 }, { stereo: loop })[0];
  log('noise Tone -10 vs +10 changes the sound (orig: no effect)', differs(a, b) ? 'yes' : 'NO', differs(a, b)); }
{ const a = render(null, { ...GRIT, RetroNoiseOn: 1, RetroNoiseType: 1 }, { stereo: loop })[0], b = render(null, { ...GRIT, RetroNoiseOn: 1, RetroNoiseType: 2 }, { stereo: loop })[0];
  log('noise VHS differs from Tape (orig: identical)', differs(a, b) ? 'yes' : 'NO', differs(a, b)); }
{ const a = render(null, { ...GRIT, RetroMagOn: 1, RetroMagDropouts: 0, RetroMagAmt: 0 }, { stereo: loop })[0], b = render(null, { ...GRIT, RetroMagOn: 1, RetroMagDropouts: 0, RetroMagAmt: 100 }, { stereo: loop })[0];
  log('magnetic Wear 0 vs 100 changes the sound (orig: no effect)', differs(a, b) ? 'yes' : 'NO', differs(a, b)); }
{ const z = new Float32Array(SR * 2); const [l] = render(z, { ...GRIT, RetroNoiseOn: 1, RetroNoiseType: 3, RetroNoiseAmt: 100 });
  const { mag, binHz } = fftMag(l.subarray(SR / 2)); let k = 1; for (let i = 2; i < 400; i++) if (mag[i] > mag[k]) k = i;
  log('hum with Tape bypassed: spectral peak (orig: DC)', `${(k * binHz).toFixed(0)} Hz`, Math.abs(k * binHz - 60) < 3); }
{ const z = new Float32Array(SR * 3); const lv = [0, 1, 2, 3].map((t) => db(rms(render(z, { ...GRIT, RetroNoiseOn: 1, RetroNoiseType: t, RetroNoiseAmt: 100, RetroNoiseFlux: 0 })[0].subarray(SR))));
  log('noise RMS at 100%: Vinyl/Tape/VHS/Hum', lv.map((x) => x.toFixed(1)).join(' / ') + ' dB', Math.max(...lv) - Math.min(...lv) < 4); }
{ const s = sine(200, 2, -12); const [l] = render(s, { ...GRIT, RetroMagOn: 1, RetroMagAmt: 0, RetroMagDropouts: 100 });
  let jumps = 0; for (let i = 1; i < l.length; i++) if (Math.abs(l[i] - l[i - 1]) > 0.02) jumps++;
  log('dropouts 100%: clicks in 2 s (orig: 67)', `${jumps}`, jumps === 0);
  const dips = db(Math.min(...Array.from({ length: 40 }, (_, b) => rms(l.subarray(b * 2400, b * 2400 + 2400))))) - db(rms(l));
  log('dropouts 100%: deepest 50 ms dip', `${dips.toFixed(1)} dB`, dips < -4); }
{ const s = sine(1500, 0.5, -20); const [l] = render(s, { ...GRIT, RetroDistOn: 1, RetroDistAmt: 100, RetroDistType: 4 });
  log('Transformer 100%, -20 dBFS in: output peak (orig +37 dBFS)', `${db(peak(l)).toFixed(1)} dBFS`, db(peak(l)) < -6); }
for (const q of [0, 1]) { const [l] = render(s45, { ...GRIT, RetroDistOn: 1, RetroDistAmt: 100, RetroDistType: 3, RetroDistFocus: 4500, Quality: q });
  const a = aliasRatio(l.subarray(16384), 4500); log(`fuzz alias energy ${q ? 'High' : 'Eco'} (orig -16.6)`, `${a.toFixed(1)} dB`, a < (q ? -30 : -22)); }
{ const r = [96, 192, 288, 2000].map((f) => { const s = sine(f, 1, -30); const [l] = render(s, { ...GRIT, RetroWobbleOn: 1, RetroWobbleAmt: 0, RetroWobbleMix: 50 }); return gainDb(l, s); });
  log('wobble on, depth 0, mix 50%: level at 96/192/288/2k (orig comb)', r.map((x) => x.toFixed(2)).join(' / '), Math.max(...r) - Math.min(...r) < 0.3); }

// ---- SHAPE --------------------------------------------------------------------------------------
const SHAPE = { ...ALL_OFF, TransBypass: 0, TransMojoOn: 0 };
const kick = readWav(path.join(__dirname, 'signals', 'kick.wav')).channels[0];
{ const res = [0, -24].map((g) => { const s = kick.map((v) => v * Math.pow(10, g / 20)); const [l] = render(s, { ...SHAPE, TransMode: 0, TransAttack: 100 }); return db(peak(l)) - db(peak(s)); });
  log('attack +100 on kick at 0 / -24 dB: peak gain (orig 6 / 30 dB)', res.map((x) => x.toFixed(1)).join(' / ') + ' dB', Math.abs(res[0] - res[1]) < 1.5 && res[1] < 16 && res[1] > 4); }
{ const s = kick; const [l] = render(s, { ...SHAPE, TransMode: 0, TransAttack: -100 });
  log('attack -100 on kick: peak change', `${(db(peak(l)) - db(peak(s))).toFixed(1)} dB`, db(peak(l)) - db(peak(s)) < -3); }
{ const [l, r] = render(null, { ...SHAPE, TransMode: 0, TransAttack: 100, TransSustain: -100 }, { stereo: [kick, kick.map((v) => v * 0.5)] });
  let maxDev = 0; for (let i = 0; i < l.length; i++) if (Math.abs(l[i]) > 1e-3) maxDev = Math.max(maxDev, Math.abs(r[i] / l[i] - 0.5));
  log('stereo link: R/L ratio stays 0.5 (max deviation)', maxDev.toFixed(4), maxDev < 0.01); }
{ const res = [0, 1, 2].map((a) => { const [l] = render(kick, { ...SHAPE, TransMode: 0, TransAttack: 100, TransAlgo: a }); return db(peak(l)) - db(peak(kick)); });
  log('attack +100 peak gain Classic / Natural / Smooth', res.map((x) => x.toFixed(1)).join(' / ') + ' dB'); }

// ---- PUNCH --------------------------------------------------------------------------------------
const PUNCH = { ...ALL_OFF, FxBypass: 0, FxMoreOn: 0, FxAir: 0 };
for (const g of [0, -18]) { const s = kick.map((v) => v * Math.pow(10, g / 20)); const [l] = render(s, { ...PUNCH, FxBoomOn: 1 });
  log(`boom on kick at ${g} dB: fired? (orig: not at -12)`, maxDiff(l, s) > 1e-3 * Math.pow(10, g / 20) ? 'yes' : 'NO', maxDiff(l, s) > 1e-4); }
{ const a = render(null, { ...PUNCH, FxAir: -6 }, { stereo: loop })[0], b = render(null, { ...PUNCH, FxAir: 12 }, { stereo: loop })[0];
  log('Air works with Finish off (orig: no effect)', differs(a, b) ? 'yes' : 'NO', differs(a, b)); }
for (const q of [0, 1]) {
  const hot = loop.map((c) => c.map((v) => v * 2)); // +6 dB over the -3 dBFS loop
  const [l] = render(null, { ...ALL_OFF, FxBypass: 0, FxMoreAmt: 100, FxAir: 6, Quality: q }, { stereo: hot });
  const tp = db(truePeak(l)); log(`finish 100% on +3 dBFS loop: true peak ${q ? 'High' : 'Eco'} (orig +3.2 dBTP)`, `${tp.toFixed(2)} dBTP`, tp < -0.3);
  const s = sine(10700, 0.5, 6); const [l2] = render(s, { ...ALL_OFF, FxBypass: 0, FxMoreAmt: 100, Quality: q });
  const tp2 = db(truePeak(l2.subarray(4800))); log(`finish 100% on +6 dBFS 10.7 kHz sine: true peak ${q ? 'High' : 'Eco'}`, `${tp2.toFixed(2)} dBTP`, tp2 < -0.3);
}
{ const s = sine(1000, 0.5, 0); const [l] = render(s, { ...PUNCH, FxSatOn: 1, FxSatAmt: 100, FxSatMode: 0 });
  log('Console saturation 100% at 0 dBFS: bounded (orig folds over)', `${db(peak(l)).toFixed(1)} dBFS`, db(peak(l)) < 6); }

// ---- Whole chain --------------------------------------------------------------------------------
{ const out = render(null, {}, { stereo: loop }); const d = db(rms(out[0])) - db(rms(loop[0]));
  log('default settings: loop RMS change (orig +8.6 dB)', `${d.toFixed(1)} dB`, Math.abs(d) < 4); }
{ const out = render(null, { AutoGain: 1, TapeDrive: 18, FxMoreAmt: 100, FxSatOn: 1, FxSatAmt: 60 }, { stereo: loop.map((c) => Float32Array.from({ length: c.length * 3 }, (_, i) => c[i % c.length])) });
  const n = out[0].length, tail = out[0].subarray(n * 2 / 3), ref = loop[0];
  const d = db(rms(tail)) - db(rms(ref)); log('auto gain on, heavy settings: RMS change after 8 s', `${d.toFixed(1)} dB`, Math.abs(d) < 1.5); }

// ---- Robustness ---------------------------------------------------------------------------------
{ const maxAll = {};
  for (const line of execFileSync(EXE, ['--params']).toString().trim().split(String.fromCharCode(10))) { const [name, , max] = line.trim().split(' '); maxAll[name] = +max; }
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
