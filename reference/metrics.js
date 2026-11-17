// Shared measurement helpers for the analysis scripts.
'use strict';
const SR = 48000;
const db = (x) => 20 * Math.log10(Math.max(x, 1e-12));
const peak = (a) => a.reduce((m, v) => Math.max(m, Math.abs(v)), 0);
const rms = (a) => Math.sqrt(a.reduce((s, v) => s + v * v, 0) / a.length);
const maxDiff = (a, b) => a.reduce((m, v, i) => Math.max(m, Math.abs(v - b[i])), 0);
const sine = (f, sec, ampDb, sr = SR) => Float32Array.from({ length: sec * sr }, (_, i) => Math.pow(10, ampDb / 20) * Math.sin(2 * Math.PI * f * i / sr));
const render = (sig, values, sr = SR, block = 512) => renderBuffers(sr, [sig, sig], values, block);

function fftMag(x, sr = SR) { // Hann-windowed magnitude spectrum, power-of-two length
  let n = 1; while (n < x.length) n <<= 1; n >>= 1;
  const re = new Float64Array(n), im = new Float64Array(n);
  for (let i = 0; i < n; i++) re[i] = x[i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / n));
  for (let i = 1, j = 0; i < n; i++) { let b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; } }
  for (let len = 2; len <= n; len <<= 1) {
    const a = -2 * Math.PI / len;
    for (let i = 0; i < n; i += len) for (let k = 0; k < len / 2; k++) {
      const wr = Math.cos(a * k), wi = Math.sin(a * k), ur = re[i + k], ui = im[i + k];
      const vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi, vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
      re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
    }
  }
  return { mag: Array.from({ length: n / 2 }, (_, k) => Math.hypot(re[k], im[k]) / (n / 4)), binHz: sr / n };
}
// Energy in harmonics of f0 vs. everything else (aliasing/inharmonic), excluding DC region.
function aliasRatio(x, f0, sr = SR) {
  const { mag, binHz } = fftMag(x, sr);
  let harm = 0, other = 0;
  for (let k = Math.ceil(20 / binHz); k < mag.length; k++) {
    const f = k * binHz, h = f / f0, near = Math.abs(h - Math.round(h)) * f0 < 4 * binHz;
    if (near) harm += mag[k] ** 2; else other += mag[k] ** 2;
  }
  return db(Math.sqrt(other / harm));
}
function truePeak(x, os = 8) { // windowed-sinc interpolation
  let tp = 0; const taps = 16;
  for (let i = taps; i < x.length - taps; i++) for (let p = 1; p < os; p++) {
    const t = p / os; let s = 0;
    for (let k = -taps + 1; k <= taps; k++) { const d = k - t; const w = 0.5 + 0.5 * Math.cos(Math.PI * d / taps); s += x[i + k] * (Math.sin(Math.PI * d) / (Math.PI * d)) * w; }
    tp = Math.max(tp, Math.abs(s));
  }
  return Math.max(tp, peak(x));
}
module.exports = { SR, db, peak, rms, maxDiff, sine, fftMag, aliasRatio, truePeak };
