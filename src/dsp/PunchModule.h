#pragma once
#include "DspUtils.h"
#include "../Params.h"
#include "../engine/Telemetry.h"

// PUNCH (was the drum multi-FX): Boom (triggered tuned sub) -> Saturate -> Air shelf -> Finish (soft-knee
// loudness clipper with a ceiling that is true-peak safe after downsampling). Runs at the oversampled rate.
class PunchModule
{
public:
    void prepare(double baseRate, int osFactor);
    void reset();
    void process(float* L, float* R, int n, const P::Values& v);
    void setTelemetry(Telemetry* t) { telemetry = t; }

private:
    struct Voice { float phase = 0.f, env = 0.f, pitchEnv = 0.f, ramp = 0.f; bool releasing = false; };
    float saturate(int mode, float x, int c);
    void updateAir(bool force);

    double fs = 192000.0;
    dsp::Biquad trigLp, air[2], bloomPre[2], bloomPost[2];
    dsp::DcBlocker dc[2];
    float trigFast = 0.f, trigSlow = 0.f;
    int holdoff = 0, holdoffLen = 0;
    Voice voices[2];
    int nextVoice = 0;
    float peCoef = 0.f, releaseCoef = 0.f, rampStep = 0.f, trigFastAtt = 0, trigFastRel = 0, trigSlowAtt = 0, trigSlowRel = 0;
    dsp::Smoother wet, boomAmt, boomPitch, boomDecay, boomSens, satAmt, airDb, moreAmt;
    dsp::Fade boomOn, satOn, moreOn;
    dsp::SwitchFade satMode;
    float ceiling = 0.891f;
    int airCounter = 0, ctrlCounter = 0;
    float trigRatio = 4.f, decCoef = 0.f, satDrive = 1.f, satComp = 1.f, finishGain = 1.f;
    Telemetry* telemetry = nullptr;
    bool first = true;
};
