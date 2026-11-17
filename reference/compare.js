// Compares two renders: aligns them by cross-correlation, then reports RMS/peak level difference,
// per-octave-band level difference, and null depth.
// Usage: node compare.js a.wav b.wav [labelA] [labelB]
'use strict';
const { readWav } = require('./wav');

const db = (x) => 20 * Math.log10(Math.max(x, 1e-12));
const rms = (a) => Math.sqrt(a.reduce((s, v) => s + v * v, 0) / Math.max(1, a.length));
const peak = (a) => a.reduce((m, v) => Math.max(m, Math.abs(v)), 0);

function bestLag(a, b, maxLag = 2000) { // lag such that b[i + lag] ~ a[i]
  const step = 4, n = Math.min(a.length, b.length) - maxLag;
  let best = 0, bestVal = -Infinity;
  for (let lag = -maxLag; lag <= maxLag; lag++) {
    let s = 0;
    for (let i = maxLag; i < n; i += step) s += a[i] * b[i + lag];
    if (s > bestVal) { bestVal = s; best = lag; }
  }
  return best;
}

// Octave bands via 2nd-order bandpass (RBJ, Q = 1.41)
function bandLevels(x, sr, centers) {
  return centers.map((fc) => {
    const w = 2 * Math.PI * fc / sr, al = Math.sin(w) / (2 * 1.414), c = Math.cos(w), a0 = 1 + al;
    const b0 = al / a0, b2 = -al / a0, a1 = -2 * c / a0, a2 = (1 - al) / a0;
    let z1 = 0, z2 = 0, s = 0;
    for (const v of x) { const y = b0 * v + z1; z1 = -a1 * y + z2; z2 = b2 * v - a2 * y; s += y * y; }
    return db(Math.sqrt(s / x.length));
  });
}

function compare(fileA, fileB, la = 'A', lb = 'B') {
  const A = readWav(fileA), B = readWav(fileB), sr = A.sampleRate;
  const a = A.channels[0], b = B.channels[0];
  const lag = bestLag(a, b);
  const n = Math.min(a.length, b.length) - Math.abs(lag) - 1;
  const aa = a.subarray(Math.max(0, -lag), Math.max(0, -lag) + n), bb = b.subarray(Math.max(0, lag), Math.max(0, lag) + n);
  const diff = aa.map((v, i) => v - bb[i]);
  const centers = [31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000];
  const ba = bandLevels(aa, sr, centers), bbL = bandLevels(bb, sr, centers);
  const lines = [];
  lines.push(`alignment: ${lb} lags ${la} by ${lag} samples (${(lag / sr * 1000).toFixed(2)} ms)`);
  lines.push(`RMS  ${la}: ${db(rms(aa)).toFixed(2)} dB   ${lb}: ${db(rms(bb)).toFixed(2)} dB   diff ${(db(rms(bb)) - db(rms(aa))).toFixed(2)} dB`);
  lines.push(`peak ${la}: ${db(peak(aa)).toFixed(2)} dB   ${lb}: ${db(peak(bb)).toFixed(2)} dB`);
  lines.push(`null depth (residual vs ${la}): ${(db(rms(diff)) - db(rms(aa))).toFixed(1)} dB`);
  lines.push('band (Hz):  ' + centers.map((c) => String(c).padStart(6)).join(''));
  lines.push(`${lb}-${la} dB:  ` + centers.map((c, i) => (bbL[i] - ba[i]).toFixed(1).padStart(6)).join(''));
  return lines.join('\n');
}

if (require.main === module) {
  const [a, b, la, lb] = process.argv.slice(2);
  console.log(compare(a, b, la || 'orig', lb || 'pulse'));
}
module.exports = { compare };
