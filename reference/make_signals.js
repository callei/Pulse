// Generates deterministic stereo test signals (32-bit float WAV) into reference/signals/.
// Usage: node make_signals.js [sampleRate=48000]
'use strict';
const path = require('path');
const fs = require('fs');
const { writeWav } = require('./wav');

const sr = +(process.argv[2] || 48000);
const outDir = path.join(__dirname, 'signals');
fs.mkdirSync(outDir, { recursive: true });

let seed = 12345;
const rnd = () => { seed ^= seed << 13; seed >>>= 0; seed ^= seed >>> 17; seed ^= seed << 5; seed >>>= 0; return seed / 4294967296 * 2 - 1; };
const buf = (sec) => new Float32Array(Math.round(sec * sr));
const db = (d) => Math.pow(10, d / 20);

function kickAt(out, t0, gain = 1) {
  const s0 = Math.round(t0 * sr); let ph = 0;
  for (let i = 0; s0 + i < out.length && i < 0.6 * sr; i++) {
    const t = i / sr, f = 48 + 110 * Math.exp(-t / 0.035);
    ph += 2 * Math.PI * f / sr;
    const click = Math.exp(-t / 0.002) * rnd() * 0.3;
    out[s0 + i] += gain * (Math.sin(ph) * Math.exp(-t / 0.28) + click);
  }
}
function snareAt(out, t0, gain = 1) {
  const s0 = Math.round(t0 * sr); let ph = 0, lp = 0;
  for (let i = 0; s0 + i < out.length && i < 0.4 * sr; i++) {
    const t = i / sr; ph += 2 * Math.PI * 190 / sr;
    const n = rnd(); lp += 0.5 * (n - lp); const hp = n - lp;
    out[s0 + i] += gain * (0.55 * Math.sin(ph) * Math.exp(-t / 0.06) + 0.6 * hp * Math.exp(-t / 0.12));
  }
}
function hatAt(out, t0, gain = 1) {
  const s0 = Math.round(t0 * sr); let prev = 0;
  for (let i = 0; s0 + i < out.length && i < 0.08 * sr; i++) {
    const n = rnd(), hp = n - prev; prev = n;
    out[s0 + i] += gain * 0.25 * hp * Math.exp(-i / sr / 0.02);
  }
}
const stereo = (m, gl = 1, gr = 1) => [m.map((v) => v * gl), m.map((v) => v * gr)];
const norm = (chs, peakDb) => { let p = 0; chs.forEach((c) => c.forEach((v) => { p = Math.max(p, Math.abs(v)); })); const g = db(peakDb) / p; return chs.map((c) => c.map((v) => v * g)); };

const sigs = {};
{ const k = buf(1.0); kickAt(k, 0.05); sigs.kick = norm(stereo(k), -6); }
{ const s = buf(0.8); snareAt(s, 0.05); sigs.snare = norm(stereo(s), -6); }
{ // 2 bars at 120 BPM, slight stereo on hats
  const L = buf(4.2), R = buf(4.2), beat = 0.5;
  for (let b = 0; b < 8; b++) {
    const t = 0.05 + b * beat;
    if (b % 4 === 0 || b % 4 === 2) { kickAt(L, t); kickAt(R, t); }
    if (b % 4 === 3) { kickAt(L, t + beat / 2, 0.8); kickAt(R, t + beat / 2, 0.8); }
    if (b % 2 === 1) { snareAt(L, t, 0.9); snareAt(R, t, 0.9); }
    for (let h = 0; h < 2; h++) { hatAt(L, t + h * beat / 2, 0.9); hatAt(R, t + h * beat / 2, 0.6); }
  }
  sigs.loop = norm([L, R], -3);
}
{ // log sine sweep 20 Hz -> 20 kHz, 4 s, -12 dBFS
  const T = 4, s = buf(T + 0.2), f1 = 20, f2 = Math.min(20000, sr * 0.45), K = T / Math.log(f2 / f1);
  for (let i = 0; i < T * sr; i++) { const t = i / sr; const fade = Math.min(1, i / (0.01 * sr), (T * sr - i) / (0.01 * sr)); s[i] = db(-12) * fade * Math.sin(2 * Math.PI * f1 * K * (Math.exp(t / K) - 1)); }
  sigs.sweep = stereo(s);
}
{ const s = buf(1.0); s[Math.round(0.01 * sr)] = db(-6); sigs.impulse = stereo(s); }
{ const s = buf(2.0); for (let i = 0; i < s.length; i++) s[i] = db(-12) * Math.sin(2 * Math.PI * 1000 * i / sr); sigs.sine1k = stereo(s); }
{ const s = buf(2.0); sigs.silence = stereo(s); }

for (const [name, chs] of Object.entries(sigs)) writeWav(path.join(outDir, name + '.wav'), sr, chs);
console.log('wrote', Object.keys(sigs).join(', '), 'at', sr, 'Hz to', outDir);
