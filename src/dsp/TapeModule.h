#pragma once
#include "DspUtils.h"
#include "../Params.h"

// TAPE: pre-emphasis -> biased tanh saturation (level-compensated) -> de-emphasis -> head bump ->
// speed-dependent HF loss -> wow & flutter (modulated delay) -> hiss. Runs at the oversampled rate.
// Adds a constant delay of baseDelaySamples() (part of the plugin latency), also on the dry/bypass path.
class TapeModule
{
public:
    static constexpr float kRefLevel = 0.25f; // -12 dBFS peak: drive is level-matched here
    void prepare(double baseRate, int osFactor);
    void reset(uint32_t seed);
    void process(float* L, float* R, int n, const P::Values& v);
    int baseDelaySamples() const { return baseDelayOS; } // at the oversampled rate (= base-rate samples * osFactor)
    static int baseDelayBaseRate(double baseRate) { return (int) std::lround(0.00125 * baseRate); }

private:
    struct Channel
    {
        dsp::Biquad pre, de, bump, lpf, hissHp, hissLp;
        dsp::DcBlocker dc;
        dsp::DelayLine mod, dry;
        dsp::SmoothRandom jitter;
    };
    void updateFilters();

    double fs = 192000.0;
    int baseDelayOS = 240;
    Channel ch[2];
    dsp::Random rng;
    dsp::Smoother drive, k, emph, bias, bumpDb, bumpHz, lpHz, wow, flutter, hiss, wet;
    float phWow[2] {}, phFl[2] {};
    dsp::SmoothRandom drift;
    int coefCounter = 0;
    bool filtersReady = false;
    float hissNorm = 0.f;
    bool wetWasOff = true;
};
