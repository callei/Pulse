#pragma once
#include "DspUtils.h"
#include "../Params.h"
#include "../engine/Telemetry.h"

// SHAPE (transient shaper): dual-envelope differential detection in the dB domain (level-independent),
// stereo-linked, followed by the MOJO character stage. "Natural" detects from a 2 ms look-ahead.
// The audio path always carries that 2 ms delay so latency is constant.
class ShapeModule
{
public:
    void prepare(double baseRate, int osFactor);
    void reset();
    void process(float* L, float* R, int n, const P::Values& v);
    void setTelemetry(Telemetry* t) { telemetry = t; }
    static int lookaheadBaseRate(double baseRate) { return (int) std::lround(0.002 * baseRate); }

private:
    void updateMojoFilter(int type);

    double fs = 192000.0;
    int lookahead = 384;
    dsp::DelayLine line[2];
    dsp::Biquad det[2], mojo[2];
    float fastEnv = 0.f, slowEnv = 0.f, gainDb = 0.f, gLin = 1.f, gStep = 0.f;
    int ctrlInterval = 4, ctrlCounter = 0;
    dsp::Smoother wet, mix, attack, sustain, mojoAmt;
    dsp::Fade mojoOn;
    dsp::SwitchFade mojoType;
    int mode = -1, algo = -1, mojoFilterType = -1;
    float fAtt = 0, fRel = 0, sAtt = 0, sRel = 0, gUp = 0, gDown = 0;
    Telemetry* telemetry = nullptr;
    int teleCounter = 0, teleInterval = 960;
    float teleMax = 0.f;
    bool first = true;
};
