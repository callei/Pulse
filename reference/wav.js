// Minimal WAV read/write (PCM 16/24/32-bit int and 32-bit float). Channels returned as Float32Array[].
'use strict';
const fs = require('fs');

function readWav(path) {
  const b = fs.readFileSync(path);
  if (b.toString('ascii', 0, 4) !== 'RIFF' || b.toString('ascii', 8, 12) !== 'WAVE') throw new Error('not a WAV: ' + path);
  let pos = 12, fmt = null, data = null;
  while (pos + 8 <= b.length) {
    const id = b.toString('ascii', pos, pos + 4), size = b.readUInt32LE(pos + 4);
    if (id === 'fmt ') fmt = { format: b.readUInt16LE(pos + 8), channels: b.readUInt16LE(pos + 10), sampleRate: b.readUInt32LE(pos + 12), bits: b.readUInt16LE(pos + 22) };
    if (id === 'data') data = b.subarray(pos + 8, pos + 8 + size);
    pos += 8 + size + (size & 1);
  }
  if (!fmt || !data) throw new Error('missing fmt/data chunk');
  let { format, channels, sampleRate, bits } = fmt;
  if (format === 0xFFFE) format = bits === 32 && data.length ? (bits === 32 ? 3 : 1) : 1; // extensible: assume float for 32-bit
  const bps = bits / 8, frames = Math.floor(data.length / (bps * channels));
  const ch = Array.from({ length: channels }, () => new Float32Array(frames));
  for (let i = 0; i < frames; i++)
    for (let c = 0; c < channels; c++) {
      const o = (i * channels + c) * bps;
      let v;
      if (format === 3) v = data.readFloatLE(o);
      else if (bits === 16) v = data.readInt16LE(o) / 32768;
      else if (bits === 24) v = data.readIntLE(o, 3) / 8388608;
      else if (bits === 32) v = data.readInt32LE(o) / 2147483648;
      else throw new Error('unsupported bit depth ' + bits);
      ch[c][i] = v;
    }
  return { sampleRate, channels: ch };
}

function writeWav(path, sampleRate, channels) {
  const n = channels.length, frames = channels[0].length;
  const b = Buffer.alloc(44 + frames * n * 4);
  b.write('RIFF', 0); b.writeUInt32LE(36 + frames * n * 4, 4); b.write('WAVE', 8);
  b.write('fmt ', 12); b.writeUInt32LE(16, 16); b.writeUInt16LE(3, 20); b.writeUInt16LE(n, 22);
  b.writeUInt32LE(sampleRate, 24); b.writeUInt32LE(sampleRate * n * 4, 28); b.writeUInt16LE(n * 4, 32); b.writeUInt16LE(32, 34);
  b.write('data', 36); b.writeUInt32LE(frames * n * 4, 40);
  for (let i = 0; i < frames; i++) for (let c = 0; c < n; c++) b.writeFloatLE(channels[c][i], 44 + (i * n + c) * 4);
  fs.writeFileSync(path, b);
}

module.exports = { readWav, writeWav };
