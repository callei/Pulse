#include "ShapeModule.h"

namespace
{
struct ModeTuning { float bpHz, fastAtt, fastRel, slowAtt, slowRel; };
// Detector tuning per source (from the original): band-pass centre and envelope times in ms.
constexpr ModeTuning kModes[3] = {
    { 90.f, 1.0f, 50.f, 25.f, 250.f },   // Kick
    { 2200.f, 0.3f, 25.f, 15.f, 150.f }, // Snare
    { 1000.f, 0.5f, 30.f, 20.f, 180.f }, // Bus
};
struct AlgoTuning { float upMs, downMs; bool useLookahead; };
constexpr AlgoTuning kAlgos[3] = {
    { 0.5f, 15.f, false }, // Classic: fast and punchy
    { 2.0f, 30.f, true },  // Natural: look-ahead, cleanest onsets
    { 5.0f, 60.f, false }, // Smooth: gentle
};
constexpr float kMaxAttackDb = 15.f, kMaxSustainDb = 12.f, kRangeDb = 12.f;
} // namespace

void ShapeModule::prepare(double baseRate, int osFactor)
{
    fs = baseRate * osFactor;
    lookahead = lookaheadBaseRate(baseRate) * osFactor;
    ctrlInterval = osFactor;
    for (auto& l : line) l.prepare(lookahead + 8);
    for (auto* s : { &wet, &mix, &attack, &sustain, &mojoAmt }) s->prepare(fs, 30.f);
    mojoOn.prepare(fs);
    mojoType.prepare(fs);
    teleInterval = std::max(1, (int) (fs / 200.0));
}

void ShapeModule::reset()
{
    for (auto& l : line) l.reset();
    for (auto& b : det) b.reset();
    for (auto& b : mojo) b.reset();
    fastEnv = slowEnv = gainDb = 0.f;
    gLin = 1.f; gStep = 0.f; ctrlCounter = 0;
    mojoOn.reset(); mojoType.reset();
    mode = algo = mojoFilterType = -1;
    teleCounter = 0; teleMax = 0.f;
    first = true;
}

void ShapeModule::updateMojoFilter(int type)
{
    for (auto& b : mojo)
    {
        if (type == 0) b.peak(3500.0, 1.2, 4.0, fs);        // Punch: upper-mid crack
        else if (type == 1) b.lowShelf(120.0, 3.5, fs);     // Body: low weight
        else b.highShelf(8000.0, -3.0, fs);                 // Air: de-harsh the top
    }
    mojoFilterType = type;
}

void ShapeModule::process(float* L, float* R, int n, const P::Values& v)
{
    auto set = [this](dsp::Smoother& s, float t) { if (first) s.snap(t); else s.set(t); };
    set(wet, v[P::TransBypass] >= 0.5f ? 0.f : 1.f);
    set(mix, v[P::TransMix] * 0.01f);
    set(attack, v[P::TransAttack] * 0.01f);
    set(sustain, v[P::TransSustain] * 0.01f);
    set(mojoAmt, v[P::TransMojoAmt] * 0.01f);
    mojoOn.set(v[P::TransMojoOn] >= 0.5f ? 1.f : 0.f);
    mojoType.request(juce::jlimit(0, 2, (int) v[P::TransMojoType]));
    first = false;

    const int newMode = juce::jlimit(0, 2, (int) v[P::TransMode]);
    const int newAlgo = juce::jlimit(0, 2, (int) v[P::TransAlgo]);
    if (newMode != mode)
    {
        mode = newMode;
        const auto& t = kModes[mode];
        for (auto& b : det) b.bandpass(t.bpHz, 0.707, fs);
        fAtt = dsp::onePoleCoef(t.fastAtt, fs); fRel = dsp::onePoleCoef(t.fastRel, fs);
        sAtt = dsp::onePoleCoef(t.slowAtt, fs); sRel = dsp::onePoleCoef(t.slowRel, fs);
    }
    if (newAlgo != algo)
    {
        algo = newAlgo;
        gUp = dsp::onePoleCoef(kAlgos[algo].upMs, fs / ctrlInterval);
        gDown = dsp::onePoleCoef(kAlgos[algo].downMs, fs / ctrlInterval);
    }
    const bool useLookahead = kAlgos[algo].useLookahead;

    for (int i = 0; i < n; ++i)
    {
        line[0].push(L[i]); line[1].push(R[i]);
        const float dL = line[0].readInt(lookahead), dR = line[1].readInt(lookahead);
        const float w = wet.next();
        if (! (w > 0.f || wet.isSmoothing())) { L[i] = dL; R[i] = dR; continue; }

        // Detection (stereo-linked): Natural looks at the undelayed signal = 2 ms into the future.
        const float sL = useLookahead ? L[i] : dL, sR = useLookahead ? R[i] : dR;
        const float a = std::max(std::abs(det[0].process(sL)), std::abs(det[1].process(sR)));
        fastEnv = a + (a > fastEnv ? fAtt : fRel) * (fastEnv - a);
        slowEnv = a + (a > slowEnv ? sAtt : sRel) * (slowEnv - a);
        const float atkAmt = attack.next(), susAmt = sustain.next();

        // Gain computer at control rate (= base sample rate); the linear gain is interpolated in between.
        if (--ctrlCounter <= 0)
        {
            ctrlCounter = ctrlInterval;
            const float diffDb = dsp::gainToDb((fastEnv + 1.0e-9f) / (slowEnv + 1.0e-9f));
            const float atk = dsp::clampf(diffDb / kRangeDb, 0.f, 1.f);
            const float sus = dsp::clampf(-diffDb / kRangeDb, 0.f, 1.f);
            const float gate = dsp::clampf((dsp::gainToDb(slowEnv + 1.0e-9f) + 75.f) / 10.f, 0.f, 1.f); // ignore the noise floor
            const float target = (atkAmt * atk * kMaxAttackDb + susAmt * sus * kMaxSustainDb) * gate;
            gainDb = target + (std::abs(target) > std::abs(gainDb) ? gUp : gDown) * (gainDb - target); // fast when moving away from 0 dB
            gStep = (dsp::dbToGain(gainDb) - gLin) / (float) ctrlInterval;
        }
        gLin += gStep;
        float yL = dL * gLin, yR = dR * gLin;

        // MOJO character stage
        const float typeFade = mojoType.next();
        if (mojoType.current != mojoFilterType) updateMojoFilter(mojoType.current);
        const float mOn = mojoOn.next(), mAmt = mojoAmt.next() * mOn * typeFade;
        if (mojoOn.active())
        {
            const float fL = mojo[0].process(yL), fR = mojo[1].process(yR);
            yL += (dsp::fastTanh(1.3f * fL) / 1.3f - yL) * mAmt;
            yR += (dsp::fastTanh(1.3f * fR) / 1.3f - yR) * mAmt;
        }

        const float m = mix.next() * w;
        L[i] = dL + (yL - dL) * m;
        R[i] = dR + (yR - dR) * m;

        // Gain trace for the UI (largest deviation per ~5 ms)
        if (std::abs(gainDb) > std::abs(teleMax)) teleMax = gainDb;
        if (++teleCounter >= teleInterval)
        {
            if (telemetry) telemetry->pushShapeGain(teleMax * m);
            teleCounter = 0; teleMax = 0.f;
        }
    }
}
