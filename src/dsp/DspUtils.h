#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

// Small, allocation-free DSP building blocks. Anything that allocates does so only in prepare().
namespace dsp
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float dbToGain(float db) { return std::pow(10.f, db * 0.05f); }
inline float gainToDb(float g) { return 20.f * std::log10(std::max(g, 1.0e-9f)); }
inline float clampf(float x, float lo, float hi) { return std::min(hi, std::max(lo, x)); }

// sin(2*pi*phase) for phase in [0, 1), polynomial (error < 5e-6). For LFOs and tone generators.
inline float fastSin01(float phase)
{
    float t = phase - 0.5f;                   // [-0.5, 0.5): sin(2*pi*phase) = -sin(2*pi*t)
    if (t > 0.25f) t = 0.5f - t;
    else if (t < -0.25f) t = -0.5f - t;
    const float x = t * kTwoPi, x2 = x * x;
    return -x * (1.f + x2 * (-1.f / 6.f + x2 * (1.f / 120.f + x2 * (-1.f / 5040.f + x2 * (1.f / 362880.f)))));
}

// Smooth near-hard clipper: u / (1 + u^4)^(1/4). Bounded to +/-1.
inline float hardishClip(float u) { const float u2 = u * u; return u / std::sqrt(std::sqrt(1.f + u2 * u2)); }

// Rational tanh approximation (max error ~1e-5 on |x| < 5, continuous with the clamp at +/-5).
inline float fastTanh(float x)
{
    x = clampf(x, -5.f, 5.f);
    const float x2 = x * x;
    return x * (135135.f + x2 * (17325.f + x2 * (378.f + x2))) / (135135.f + x2 * (62370.f + x2 * (3150.f + 28.f * x2)));
}

// One-pole coefficient for a time constant in ms.
inline float onePoleCoef(float ms, double sr) { return ms <= 0.f ? 0.f : (float) std::exp(-1000.0 / (ms * sr)); }

// Linear ramp smoother (20-50 ms), cheap and exact at target.
struct Smoother
{
    float current = 0.f, target = 0.f, step = 0.f;
    int remaining = 0, rampLen = 1;
    void prepare(double sr, float ms) { rampLen = std::max(1, (int) (sr * ms * 0.001)); }
    void snap(float v) { current = target = v; remaining = 0; step = 0.f; }
    void set(float v)
    {
        if (v == target) return;
        target = v; remaining = rampLen; step = (target - current) / (float) rampLen;
    }
    float next()
    {
        if (remaining > 0) { current += step; if (--remaining == 0) current = target; }
        return current;
    }
    bool isSmoothing() const { return remaining > 0; }
};

// Click-free switching of a discrete choice: fades the effect out, swaps the choice, fades back in.
struct SwitchFade
{
    int current = -1, pending = 0;
    float g = 1.f, step = 0.01f;
    void prepare(double sr, float msEachWay = 15.f) { step = 1.f / std::max(1.f, (float) (sr * msEachWay * 0.001)); }
    void reset() { current = -1; g = 1.f; }
    void request(int v) { pending = v; if (current < 0) current = v; }
    float next() // returns the effect gain; 'current' changes when the gain reaches zero
    {
        if (pending != current) { g -= step; if (g <= 0.f) { g = 0.f; current = pending; } }
        else if (g < 1.f) g = std::min(1.f, g + step);
        return g;
    }
};

// On/off fade (30 ms), so toggling an effect never clicks. active() is false only when fully off.
struct Fade
{
    Smoother s;
    bool init = false;
    void prepare(double sr, float ms = 30.f) { s.prepare(sr, ms); }
    void reset() { init = false; }
    void set(float target) { if (! init) { s.snap(target); init = true; } else s.set(target); }
    float next() { return s.next(); }
    bool active() const { return s.current > 0.f || s.isSmoothing(); }
};

// xorshift32: deterministic, seedable noise in [-1, 1).
struct Random
{
    uint32_t s = 0x9E3779B9u;
    void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; }
    uint32_t nextU() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float bipolar() { return (float) nextU() * (2.0f / 4294967296.0f) - 1.0f; }
    float unipolar() { return (float) nextU() * (1.0f / 4294967296.0f); }
};

// Smooth random LFO: random targets joined by cosine interpolation at a given rate (Hz).
struct SmoothRandom
{
    float a = 0.f, b = 0.f, phase = 1.f;
    float next(Random& rng, float rateHz, double sr)
    {
        phase += (float) (rateHz / sr);
        if (phase >= 1.f) { phase -= std::floor(phase); a = b; b = rng.bipolar(); }
        const float t = phase * phase * (3.f - 2.f * phase); // smoothstep
        return a + (b - a) * t;
    }
    void reset() { a = b = 0.f; phase = 1.f; }
};

// RBJ biquad, transposed direct form II, double precision (low cutoffs at 4x-oversampled rates need it).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    float process(float xf)
    {
        const double x = xf, y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() { z1 = z2 = 0.0; }
    void set(double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0;
        a1 = A1 / A0; a2 = A2 / A0;
    }
    static double w0(double f, double sr) { return 2.0 * 3.141592653589793 * std::min(f, sr * 0.49) / sr; }

    void lowpass(double f, double q, double sr)
    {
        const double w = w0(f, sr), c = std::cos(w), al = std::sin(w) / (2 * q);
        set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass(double f, double q, double sr)
    {
        const double w = w0(f, sr), c = std::cos(w), al = std::sin(w) / (2 * q);
        set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass(double f, double q, double sr) // constant 0 dB peak gain
    {
        const double w = w0(f, sr), c = std::cos(w), al = std::sin(w) / (2 * q);
        set(al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak(double f, double q, double db, double sr)
    {
        const double A = std::pow(10.0, db / 40), w = w0(f, sr), c = std::cos(w), al = std::sin(w) / (2 * q);
        set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void highShelf(double f, double db, double sr, double S = 1.0)
    {
        const double A = std::pow(10.0, db / 40), w = w0(f, sr), c = std::cos(w);
        const double al = std::sin(w) / 2 * std::sqrt((A + 1 / A) * (1 / S - 1) + 2), sA = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
            (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    }
    void lowShelf(double f, double db, double sr, double S = 1.0)
    {
        const double A = std::pow(10.0, db / 40), w = w0(f, sr), c = std::cos(w);
        const double al = std::sin(w) / 2 * std::sqrt((A + 1 / A) * (1 / S - 1) + 2), sA = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
            (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
    }
};

// First-order DC blocker (~10 Hz).
struct DcBlocker
{
    float r = 0.999f, x1 = 0.f, y1 = 0.f;
    void prepare(double sr, float hz = 10.f) { r = (float) std::exp(-2.0 * 3.141592653589793 * hz / sr); }
    float process(float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.f; }
};

// One-pole low-pass (for smoothing/hiss shaping).
struct OnePoleLP
{
    float a = 0.f, z = 0.f;
    void setCutoff(double hz, double sr) { a = (float) std::exp(-2.0 * 3.141592653589793 * hz / sr); }
    float process(float x) { z = x + a * (z - x); return z; }
    void reset() { z = 0.f; }
};

// Circular delay line with fractional (4-point Hermite) read. Size fixed in prepare().
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare(int maxDelaySamples)
    {
        int n = 1;
        while (n < maxDelaySamples + 4) n <<= 1;
        buf.assign((size_t) n, 0.f);
        mask = n - 1; w = 0;
    }
    void reset() { std::fill(buf.begin(), buf.end(), 0.f); w = 0; }
    void push(float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float readInt(int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
    // d = delay in samples (>= 1) measured from the most recently pushed sample
    float readFrac(float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float xm1 = readInt(i - 1), x0 = readInt(i), x1 = readInt(i + 1), x2 = readInt(i + 2);
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

// Linear-phase fractional delay (Blackman-windowed sinc, 32 taps) for exact latency alignment.
// prepare(D) sets a total delay of D samples (D >= kCentre + 1, may be fractional).
struct FracDelay
{
    static constexpr int kTaps = 32, kCentre = kTaps / 2 - 1;
    float h[kTaps] {};
    int intDelay = 0;
    DelayLine line;

    void prepare(double D)
    {
        const double rem = D - kCentre;
        intDelay = (int) std::floor(rem);
        const double frac = rem - intDelay, centre = kCentre + frac;
        double sum = 0;
        for (int k = 0; k < kTaps; ++k)
        {
            const double t = k - centre, x = 3.141592653589793 * t;
            const double sinc = std::abs(t) < 1e-9 ? 1.0 : std::sin(x) / x;
            const double wpos = (k - centre + kTaps * 0.5) / kTaps; // 0..1 across the window, centred on 'centre'
            const double w = (wpos <= 0 || wpos >= 1) ? 0.0 : 0.42 - 0.5 * std::cos(2 * 3.141592653589793 * wpos) + 0.08 * std::cos(4 * 3.141592653589793 * wpos);
            h[k] = (float) (sinc * w);
            sum += h[k];
        }
        for (auto& c : h) c = (float) (c / sum);
        line.prepare(intDelay + kTaps + 4);
    }
    void reset() { line.reset(); }
    void process(float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            line.push(x[i]);
            float y = 0.f;
            for (int k = 0; k < kTaps; ++k) y += h[k] * line.readInt(intDelay + k);
            x[i] = y;
        }
    }
};
} // namespace dsp
