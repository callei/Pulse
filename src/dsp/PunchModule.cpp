#include "PunchModule.h"

namespace
{
constexpr float kKnee = 0.8f; // finisher: linear below 80% of the ceiling, smooth curve above

// Soft-knee clipper normalised to a ceiling of 1.0
inline float kneeClip(float u)
{
    const float a = std::abs(u);
    if (a <= kKnee) return u;
    const float y = kKnee + (1.f - kKnee) * dsp::fastTanh((a - kKnee) / (1.f - kKnee));
    return u < 0.f ? -y : y;
}
} // namespace

void PunchModule::prepare(double baseRate, int osFactor)
{
    fs = baseRate * osFactor;
    trigLp.lowpass(150.0, 0.707, fs);
    for (int c = 0; c < 2; ++c)
    {
        dc[c].prepare(fs, 5.f);
        bloomPre[c].lowShelf(120.0, 5.0, fs);
        bloomPost[c].lowShelf(120.0, -5.0, fs);
    }
    trigFastAtt = dsp::onePoleCoef(0.3f, fs); trigFastRel = dsp::onePoleCoef(20.f, fs);
    trigSlowAtt = dsp::onePoleCoef(60.f, fs); trigSlowRel = dsp::onePoleCoef(400.f, fs);
    holdoffLen = (int) (0.08 * fs);
    peCoef = dsp::onePoleCoef(25.f, fs);
    releaseCoef = dsp::onePoleCoef(4.f, fs);
    rampStep = (float) (1.0 / (0.0005 * fs));
    for (auto* s : { &wet, &boomAmt, &boomPitch, &boomDecay, &boomSens, &satAmt, &airDb, &moreAmt }) s->prepare(fs, 30.f);
    for (auto* f : { &boomOn, &satOn, &moreOn }) f->prepare(fs);
    satMode.prepare(fs);
    // Ceiling inside the oversampled domain, chosen so the downsampled true peak stays below -0.3 dBTP
    // (verified by reference/analyze_pulse.js). 2x needs more margin than 4x.
    ceiling = dsp::dbToGain(osFactor >= 4 ? -0.8f : -1.2f);
}

void PunchModule::reset()
{
    trigLp.reset();
    for (int c = 0; c < 2; ++c) { air[c].reset(); bloomPre[c].reset(); bloomPost[c].reset(); dc[c].reset(); }
    trigFast = trigSlow = 0.f;
    holdoff = 0;
    for (auto& vc : voices) vc = Voice {};
    for (auto* f : { &boomOn, &satOn, &moreOn }) f->reset();
    satMode.reset();
    ctrlCounter = 0;
    first = true;
}

void PunchModule::updateAir(bool force)
{
    if (! force && ! (airDb.isSmoothing() && --airCounter <= 0)) return;
    airCounter = 32;
    for (auto& a : air) a.highShelf(10000.0, airDb.current, fs, 0.8);
}

// Static transfer curves (no state), also used to compute level compensation.
static float satCurve(int mode, float x)
{
    switch (mode)
    {
        case 0: { const float u = dsp::clampf(x, -1.f, 1.f); return 1.5f * (u - u * u * u / 3.f); } // Console: smooth cubic
        case 2: return 0.9f * dsp::hardishClip(x / 0.9f);                                           // Digital: firm clip
        case 3: return x > 0.f ? x / (1.f + x) : x / (1.f - 0.5f * x);                              // Tube: asymmetric
        case 1:                                                                                     // Tape
        case 4:                                                                                     // Bloom (tanh core)
        default: return dsp::fastTanh(x);
    }
}

float PunchModule::saturate(int mode, float x, int c)
{
    if (mode == 3) return dc[c].process(satCurve(3, x));                                  // Tube: DC-blocked
    if (mode == 4) return bloomPost[c].process(satCurve(4, bloomPre[c].process(x)));      // Bloom: lows saturate first
    return satCurve(mode, x);
}

void PunchModule::process(float* L, float* R, int n, const P::Values& v)
{
    auto set = [this](dsp::Smoother& s, float t) { if (first) s.snap(t); else s.set(t); };
    set(wet, v[P::FxBypass] >= 0.5f ? 0.f : 1.f);
    set(boomAmt, v[P::FxBoomAmt] * 0.01f);
    set(boomPitch, v[P::FxBoomPitch]);
    set(boomDecay, v[P::FxBoomDecay]);
    set(boomSens, v[P::FxBoomSens] * 0.01f);
    set(satAmt, v[P::FxSatAmt] * 0.01f);
    set(airDb, v[P::FxAir]);
    set(moreAmt, v[P::FxMoreAmt] * 0.01f);
    boomOn.set(v[P::FxBoomOn] >= 0.5f ? 1.f : 0.f);
    satOn.set(v[P::FxSatOn] >= 0.5f ? 1.f : 0.f);
    moreOn.set(v[P::FxMoreOn] >= 0.5f ? 1.f : 0.f);
    satMode.request(juce::jlimit(0, 4, (int) v[P::FxSatMode]));
    if (first) { updateAir(true); first = false; }

    const float invFs = (float) (1.0 / fs);
    for (int i = 0; i < n; ++i)
    {
        const float dryL = L[i], dryR = R[i];
        const float w = wet.next();
        if (! (w > 0.f || wet.isSmoothing())) continue;
        float x[2] = { dryL, dryR };

        // --- Boom: relative (level-independent) kick detection on the low end ---
        const float bOn = boomOn.next(), bAmt = boomAmt.next(), pitch = boomPitch.next(), decayMs = boomDecay.next(), sens = boomSens.next();
        const float lo = std::abs(trigLp.process((dryL + dryR) * 0.5f));
        trigFast = lo + (lo > trigFast ? trigFastAtt : trigFastRel) * (trigFast - lo);
        trigSlow = lo + (lo > trigSlow ? trigSlowAtt : trigSlowRel) * (trigSlow - lo);
        if (holdoff > 0) --holdoff;
        const float sAmt = satAmt.next(), push = moreAmt.next();
        if (--ctrlCounter <= 0) // slow-moving values: refresh every 16 samples instead of per sample
        {
            ctrlCounter = 16;
            trigRatio = dsp::dbToGain(12.f - 9.f * sens); // jump above the running low-end level needed to fire
            decCoef = dsp::onePoleCoef(decayMs, fs);
            satDrive = dsp::dbToGain(sAmt * 18.f);
            const float ref = 0.5f; // level-matched at -6 dBFS peaks
            satComp = ref * 2.f / std::max(1e-6f, satCurve(satMode.current, ref * satDrive) - satCurve(satMode.current, -ref * satDrive));
            finishGain = dsp::dbToGain(push * 10.f);
        }
        if (boomOn.active())
        {
            if (holdoff == 0 && trigFast > 0.00316f && trigFast > trigSlow * trigRatio)
            {
                holdoff = holdoffLen;
                for (auto& vc : voices) if (vc.env > 0.f) vc.releasing = true; // fade out a ringing voice
                auto& nv = voices[nextVoice];
                nextVoice ^= 1;
                nv = Voice {};
                nv.env = 1.f; nv.pitchEnv = 1.f;
                if (telemetry) telemetry->boomCount.fetch_add(1, std::memory_order_relaxed);
            }
            float boom = 0.f;
            for (auto& vc : voices)
            {
                if (vc.env < 1.0e-4f) { vc.env = 0.f; continue; }
                vc.phase += pitch * (1.f + 1.8f * vc.pitchEnv) * invFs;
                if (vc.phase >= 1.f) vc.phase -= 1.f;
                vc.ramp = std::min(1.f, vc.ramp + rampStep);
                boom += dsp::fastTanh(1.5f * std::sin(dsp::kTwoPi * vc.phase) * vc.env * vc.ramp);
                vc.env *= vc.releasing ? releaseCoef : decCoef;
                vc.pitchEnv *= peCoef;
            }
            boom *= bAmt * 0.6f * bOn;
            x[0] += boom; x[1] += boom;
        }

        // --- Saturate (level-matched at -6 dBFS peaks, blends in over the first 20% of Amount) ---
        const float sOn = satOn.next(), modeFade = satMode.next();
        const float aOn = moreOn.next();
        airDb.next();
        updateAir(false);
        if (satOn.active())
        {
            const int mode = satMode.current;
            const float mixAmt = dsp::clampf(sAmt * 5.f, 0.f, 1.f) * sOn * modeFade;
            for (int c = 0; c < 2; ++c) x[c] += (saturate(mode, x[c] * satDrive, c) * satComp - x[c]) * mixAmt;
        }

        // --- Air: 10 kHz shelf, independent of Finish ---
        for (int c = 0; c < 2; ++c) x[c] = air[c].process(x[c]);

        // --- Finish: push into a soft-knee ceiling ---
        if (moreOn.active())
        {
            for (int c = 0; c < 2; ++c) x[c] += (ceiling * kneeClip(x[c] * finishGain / ceiling) - x[c]) * aOn;
        }

        L[i] = dryL + (x[0] - dryL) * w;
        R[i] = dryR + (x[1] - dryR) * w;
    }
}
