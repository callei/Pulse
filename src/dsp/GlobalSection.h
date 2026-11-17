#pragma once
#include "DspUtils.h"
#include "../Params.h"
#include <atomic>

// Base-rate global stages.
//   pre():  input gain, input meter, feeds the time-aligned dry path
//   post(): stereo width (M/S, 0-200%), auto-gain (loudness match), dry/wet mix, output gain, output meter
class GlobalSection
{
public:
    void prepare(double sampleRate, int dryDelaySamples, int maxBlock);
    void reset();
    void pre(float* L, float* R, int n, const P::Values& v);
    void post(float* L, float* R, int n, const P::Values& v);

    // Peak meters (linear), read-and-reset from the UI thread.
    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    std::atomic<float> autoGainDb { 0.f };

private:
    double fs = 48000.0;
    int dryDelay = 0;
    dsp::DelayLine dry[2];
    dsp::Smoother inGain, outGain, width, mix;
    float pDry = 0.f, pWet = 0.f, agGain = 1.f, agCoef = 0.f, agSmooth = 0.f;
    bool first = true;
};
