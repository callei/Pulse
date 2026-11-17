#pragma once
#include "DspUtils.h"
#include "../Params.h"

// GRIT (was "Retro Color"): Noise -> Wobble -> Distort -> Digital -> Magnetic, in series, at the oversampled rate.
// Intensity scales every stage. Flux is a smooth random LFO that moves noise level, wobble depth and drive.
// Adds a constant delay of baseDelay (wobble headroom) on every path so mixes never comb-filter.
class GritModule
{
public:
    void prepare(double baseRate, int osFactor);
    void reset(uint32_t seed);
    void process(float* L, float* R, int n, const P::Values& v);
    static int baseDelayBaseRate(double baseRate) { return (int) std::lround(0.002 * baseRate); }

private:
    struct NoiseState
    {
        dsp::OnePoleLP surfaceLp, tapeLp;
        dsp::Biquad hp, vhsBp, toneHi, toneLo;
        float crackleEnv = 0.f, crackleAmp = 0.f, crackleDecay = 0.f;
        float hpz = 0.f;
    };
    struct Channel
    {
        NoiseState noise;
        dsp::DelayLine dry, wob;
        dsp::Biquad focus, digiLp, wearLp;
        dsp::DcBlocker dc;
        float held = 0.f, digiPhase = 1.f;
    };
    float noiseSample(int c, int type, float flux);
    float shape(int type, float x) const;
    void updateFilters(bool force);

    double fs = 192000.0, baseFs = 48000.0;
    int baseDelay = 384;
    Channel ch[2];
    dsp::Random rng;
    dsp::SmoothRandom flux1, flux2, wearMod, vhsWobble;
    dsp::Smoother wet, mag, noiseAmt, tone, fluxAmt, wobAmt, wobMix, distAmt, focus, digiAmt, digiSmooth, wearAmt, dropAmt;
    dsp::Fade noiseOn, wobOn, distOn, digiOn, magOn;
    dsp::SwitchFade noiseType, distType;
    float wobPh[2] {}, humPh = 0.f, buzzPh = 0.f, whinePh = 0.f;
    float dropGain = 1.f, dropTarget = 1.f, dropCoef = 0.f;
    int dropRemaining = 0;
    float digiInc = 1.f, drive = 1.f, comp = 1.f;
    int driveCounter = 0;
    int coefCounter = 0;
    bool first = true;
};
