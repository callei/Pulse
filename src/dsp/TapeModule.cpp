#include "TapeModule.h"

namespace
{
// Per-speed head bump frequency and HF loss cutoff (from the original).
constexpr float kBumpHz[4] = { 110.f, 75.f, 55.f, 95.f };
constexpr float kLpHz[4] = { 12000.f, 16000.f, 20000.f, 9500.f };
// Per-formula saturation steepness and pre/de-emphasis amount.
constexpr float kK[3] = { 1.6f, 2.2f, 3.2f };
constexpr float kEmph[3] = { 3.0f, 4.5f, 6.0f };
constexpr float kWowMaxMs = 0.8f, kFlutterMaxMs = 0.25f;
constexpr int kCoefInterval = 32;
} // namespace

void TapeModule::prepare(double baseRate, int osFactor)
{
    fs = baseRate * osFactor;
    baseDelayOS = baseDelayBaseRate(baseRate) * osFactor;
    for (auto& c : ch)
    {
        c.mod.prepare(baseDelayOS * 2 + 8);
        c.dry.prepare(baseDelayOS + 8);
        c.dc.prepare(fs, 5.f);
    }
    for (auto* s : { &drive, &k, &emph, &bias, &bumpDb, &bumpHz, &lpHz, &wow, &flutter, &wet })
        s->prepare(fs, 30.f);
    hiss.prepare(fs, 50.f);
}

void TapeModule::reset(uint32_t seed)
{
    rng.seed(seed);
    for (auto& c : ch)
    {
        for (auto* b : { &c.pre, &c.de, &c.bump, &c.lpf, &c.hissHp, &c.hissLp }) b->reset();
        c.dc.reset(); c.mod.reset(); c.dry.reset(); c.jitter.reset();
    }
    drift.reset();
    phWow[0] = 0.f; phWow[1] = 0.25f; phFl[0] = 0.f; phFl[1] = 0.5f;
    filtersReady = false;
    wetWasOff = true;
    hissNorm = 0.f;
    // snap all smoothers to "unset" so the first process() call snaps them to the parameter values
    for (auto* s : { &drive, &k, &emph, &bias, &bumpDb, &bumpHz, &lpHz, &wow, &flutter, &hiss, &wet })
        s->snap(-1000.f);
}

void TapeModule::updateFilters()
{
    const float bi = bias.current; // -0.5 .. +0.5
    const float preDb = emph.current + std::max(0.f, -bi) * 6.f;  // under-bias: more HF into the tape
    const float deDb = -emph.current - std::max(0.f, bi) * 6.f;   // over-bias: darker playback
    for (auto& c : ch)
    {
        c.pre.highShelf(6000.0, preDb, fs);
        c.de.highShelf(6000.0, deDb, fs);
        c.bump.peak(bumpHz.current, 1.5, bumpDb.current, fs);
        c.lpf.lowpass(lpHz.current, 0.707, fs);
        c.hissHp.highpass(800.0, 0.5, fs);
        c.hissLp.lowpass(lpHz.current, 0.707, fs);
    }
}

void TapeModule::process(float* L, float* R, int n, const P::Values& v)
{
    const int speed = juce::jlimit(0, 3, (int) v[P::TapeSpeed]);
    const int formula = juce::jlimit(0, 2, (int) v[P::TapeFormula]);
    const float wetTarget = v[P::TapeBypass] >= 0.5f ? 0.f : v[P::TapeMix] * 0.01f;
    const float hissDb = v[P::TapeHiss];

    auto setOrSnap = [](dsp::Smoother& s, float t) { if (s.current <= -999.f) s.snap(t); else s.set(t); };
    setOrSnap(drive, dsp::dbToGain(v[P::TapeDrive]));
    setOrSnap(k, kK[formula]);
    setOrSnap(emph, kEmph[formula]);
    setOrSnap(bias, v[P::TapeBias] * 0.01f - 0.5f);
    setOrSnap(bumpDb, v[P::TapeHeadBump] >= 0.5f ? 2.5f : 0.f);
    setOrSnap(bumpHz, kBumpHz[speed]);
    setOrSnap(lpHz, kLpHz[speed]);
    setOrSnap(wow, v[P::TapeWow] * 0.01f * kWowMaxMs);
    setOrSnap(flutter, v[P::TapeFlutter] * 0.01f * kFlutterMaxMs);
    setOrSnap(hiss, hissDb <= -89.95f ? 0.f : dsp::dbToGain(hissDb));
    setOrSnap(wet, wetTarget);

    const float msToSamp = (float) (fs * 0.001);
    const float base = (float) baseDelayOS;
    const float invFs = (float) (1.0 / fs);
    float* io[2] = { L, R };

    for (int i = 0; i < n; ++i)
    {
        const float w = wet.next();
        const bool active = w > 0.f || wet.isSmoothing();

        // Keep the dry path delayed by the same amount as the wet path.
        float dryOut[2];
        for (int c = 0; c < 2; ++c)
        {
            ch[c].dry.push(io[c][i]);
            dryOut[c] = ch[c].dry.readInt(baseDelayOS);
        }

        if (! active)
        {
            for (int c = 0; c < 2; ++c) { ch[c].mod.push(io[c][i]); io[c][i] = dryOut[c]; }
            wetWasOff = true;
            continue;
        }
        if (wetWasOff)
        {
            for (auto& c : ch) for (auto* b : { &c.pre, &c.de, &c.bump, &c.lpf }) b->reset();
            wetWasOff = false;
            filtersReady = false;
        }

        const bool filtersMoving = emph.isSmoothing() || bias.isSmoothing() || bumpDb.isSmoothing() || bumpHz.isSmoothing() || lpHz.isSmoothing();
        const float g = drive.next(), kk = k.next(), bi = bias.next() * 0.2f;
        emph.next(); bumpDb.next(); bumpHz.next(); lpHz.next();
        const float wowMs = wow.next(), flMs = flutter.next(), hissG = hiss.next();
        if (! filtersReady || (filtersMoving && --coefCounter <= 0))
        {
            updateFilters();
            filtersReady = true;
            coefCounter = kCoefInterval;
        }

        // Level compensation: unity gain for a drum-level signal (kRefLevel peak) at any drive/formula/bias.
        // (Calibrating for quiet signals instead made hot settings drop far below the input level.)
        const float tb = dsp::fastTanh(kk * bi);
        const float comp = 2.f * kRefLevel / std::max(1.0e-6f, dsp::fastTanh(kk * (kRefLevel * g + bi)) - dsp::fastTanh(kk * (-kRefLevel * g + bi)));

        const float d = drift.next(rng, 0.5f, fs);
        if (filtersMoving || hissNorm <= 0.f) hissNorm = 1.732f * std::sqrt((float) (fs * 0.5) / std::max(1000.f, lpHz.current - 800.f));
        const float hissScale = hissG * hissNorm;

        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            const float x = io[c][i];
            float y = s.pre.process(x * g);
            y = s.dc.process((dsp::fastTanh(kk * (y + bi)) - tb) * comp); // level-matched incl. drive
            y = s.de.process(y);
            y = s.bump.process(y);
            y = s.lpf.process(y);

            // Wow & flutter
            phWow[c] += (c == 0 ? 0.8f : 0.85f) * (1.f + 0.15f * d) * invFs;
            phFl[c] += (c == 0 ? 12.f : 13.5f) * invFs;
            if (phWow[c] >= 1.f) phWow[c] -= 1.f;
            if (phFl[c] >= 1.f) phFl[c] -= 1.f;
            const float jit = s.jitter.next(rng, 30.f, fs);
            const float modMs = dsp::fastSin01(phWow[c]) * wowMs
                              + (dsp::fastSin01(phFl[c]) + 0.3f * jit) * flMs;
            s.mod.push(y);
            y = s.mod.readFrac(base + modMs * msToSamp);

            if (hissG > 0.f)
                y += s.hissLp.process(s.hissHp.process(rng.bipolar())) * hissScale;

            io[c][i] = dryOut[c] + (y - dryOut[c]) * w;
        }
    }
}
