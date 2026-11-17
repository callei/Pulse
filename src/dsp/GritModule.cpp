#include "GritModule.h"

namespace
{
constexpr float kWobMaxMs = 1.6f;      // wobble depth at 100% (headroom 2 ms incl. flux)
constexpr float kNoiseLevel = 0.12f;   // ~ -30 dBFS RMS at 100%
// Per-type gain so all noise types have a similar loudness at the same Amount (calibrated by measurement).
constexpr float kTypeGain[4] = { 4.4f, 1.4f, 2.06f, 0.54f };
constexpr int kCoefInterval = 32;

float pickParam(const P::Values& v, int id) { return v[(size_t) id]; }
} // namespace

void GritModule::prepare(double baseRate, int osFactor)
{
    baseFs = baseRate;
    fs = baseRate * osFactor;
    baseDelay = baseDelayBaseRate(baseRate) * osFactor;
    for (auto& c : ch)
    {
        c.dry.prepare(baseDelay + 8);
        c.wob.prepare(baseDelay * 2 + 8);
        c.dc.prepare(fs, 5.f);
        c.noise.surfaceLp.setCutoff(2500.0, fs);
        c.noise.tapeLp.setCutoff(9000.0, fs);
        c.noise.hp.highpass(300.0, 0.6, fs);
        c.noise.vhsBp.bandpass(2500.0, 0.7, fs);
    }
    for (auto* s : { &wet, &mag, &noiseAmt, &tone, &fluxAmt, &wobAmt, &wobMix, &distAmt, &focus, &digiAmt, &digiSmooth, &wearAmt, &dropAmt })
        s->prepare(fs, 30.f);
    for (auto* f : { &noiseOn, &wobOn, &distOn, &digiOn, &magOn }) f->prepare(fs);
    noiseType.prepare(fs);
    distType.prepare(fs);
    dropCoef = dsp::onePoleCoef(8.f, fs);
}

void GritModule::reset(uint32_t seed)
{
    rng.seed(seed * 2654435761u + 7u);
    for (auto& c : ch)
    {
        c.dry.reset(); c.wob.reset(); c.dc.reset();
        for (auto* b : { &c.focus, &c.digiLp, &c.wearLp, &c.noise.hp, &c.noise.vhsBp, &c.noise.toneHi, &c.noise.toneLo }) b->reset();
        c.noise.surfaceLp.reset(); c.noise.tapeLp.reset();
        c.noise.crackleEnv = c.noise.crackleAmp = 0.f;
        c.held = 0.f; c.digiPhase = 1.f;
    }
    for (auto* s : { &flux1, &flux2, &wearMod, &vhsWobble }) s->reset();
    for (auto* f : { &noiseOn, &wobOn, &distOn, &digiOn, &magOn }) f->reset();
    noiseType.reset(); distType.reset();
    wobPh[0] = 0.f; wobPh[1] = 0.25f; humPh = buzzPh = whinePh = 0.f;
    dropGain = dropTarget = 1.f; dropRemaining = 0; driveCounter = 0;
    first = true;
}

void GritModule::updateFilters(bool force)
{
    const bool moving = tone.isSmoothing() || focus.isSmoothing() || digiAmt.isSmoothing() || digiSmooth.isSmoothing()
                     || wearAmt.isSmoothing() || mag.isSmoothing();
    if (! force && ! (moving && --coefCounter <= 0)) return;
    coefCounter = kCoefInterval;

    const double nyq = 0.45 * baseFs;
    const float m = mag.current;
    // Digital: reduced rate and the "Smooth" low-pass that follows it
    const double rate = baseFs * std::pow(1500.0 / baseFs, (double) (digiAmt.current * m));
    digiInc = (float) (rate / fs);
    const double fHi = std::min(20000.0, nyq), fLo = std::max(500.0, rate * 0.45);
    const double smoothHz = std::exp(std::log(fHi) + (std::log(std::min(fHi, fLo)) - std::log(fHi)) * digiSmooth.current);
    // Magnetic wear: high-frequency loss from 20 kHz down to 4 kHz
    const double wearHz = std::min(nyq, 20000.0 * std::pow(0.2, (double) (wearAmt.current * m)));
    for (auto& c : ch)
    {
        c.noise.toneHi.highShelf(1500.0, tone.current * 1.2, fs);
        c.noise.toneLo.lowShelf(250.0, -tone.current * 0.6, fs);
        c.focus.bandpass(std::min((double) focus.current, nyq), 0.9, fs);
        c.digiLp.lowpass(smoothHz, 0.707, fs);
        c.wearLp.lowpass(wearHz, 0.707, fs);
    }
}

float GritModule::noiseSample(int c, int type, float fluxMod)
{
    auto& s = ch[c].noise;
    switch (type)
    {
        case 0: // Vinyl: Poisson crackle (clicks + occasional pops) over soft surface noise
        {
            const float rate = 14.f * (1.f + fluxMod); // events per second
            if (rng.unipolar() < rate / (float) fs)
            {
                const bool pop = rng.unipolar() < 0.08f;
                const float u = rng.unipolar();
                s.crackleAmp = (pop ? 1.f : 0.25f + 0.75f * u * u * u) * (rng.bipolar() < 0 ? -1.f : 1.f);
                s.crackleEnv = 1.f;
                s.crackleDecay = dsp::onePoleCoef(pop ? 1.2f : 0.05f + 0.3f * rng.unipolar(), fs);
            }
            const float click = s.crackleAmp * s.crackleEnv * (0.6f + 0.4f * rng.bipolar());
            s.crackleEnv *= s.crackleDecay;
            const float surface = s.surfaceLp.process(rng.bipolar()) * 0.35f;
            return s.hp.process(click * 2.5f + surface);
        }
        case 1: // Tape: bright, smooth hiss
            return s.hp.process(s.tapeLp.process(rng.bipolar()));
        case 2: // VHS: 59.94 Hz buzz with harmonics + band-limited noise + faint line whine
        {
            const float buzz = 2.f * buzzPh - 1.f; // shared (computed per sample in process)
            const float whine = (15734.f < 0.45f * (float) baseFs) ? dsp::fastSin01(whinePh) * 0.04f : 0.f;
            return s.hp.process(buzz * 0.35f * (1.f + 0.3f * fluxMod)) + s.vhsBp.process(rng.bipolar()) * 0.8f + whine;
        }
        case 3: // Hum: 60 Hz + harmonics, mono
        default:
            {
            const float p2 = humPh * 2.f, p3 = humPh * 3.f;
            return (dsp::fastSin01(humPh) + 0.5f * dsp::fastSin01(p2 - std::floor(p2)) + 0.25f * dsp::fastSin01(p3 - std::floor(p3))) * 0.6f;
        }
    }
}

float GritModule::shape(int type, float x) const
{
    switch (type)
    {
        case 0: return dsp::fastTanh(x);                                                    // Soft
        case 1: return dsp::fastTanh(x + 0.3f) - 0.2913126f;                                // Tube: biased (even harmonics)
        case 2: return x > 0.f ? 0.5f * dsp::fastTanh(x * 2.f) : 0.8f * dsp::fastTanh(x * 1.25f); // Diode: asymmetric knee
        case 3: return 0.85f * dsp::hardishClip(x * 2.f / 0.85f);                            // Fuzz: near-hard clip
        case 4:                                                                              // Transformer: bounded cubic
        default:
        {
            const float u = dsp::clampf(x + 0.05f, -1.5f, 1.5f);
            return (u - 0.148148f * u * u * u) - (0.05f - 0.148148f * 0.000125f);
        }
    }
}

void GritModule::process(float* L, float* R, int n, const P::Values& v)
{
    auto pv = [&](int id) { return pickParam(v, id); };
    auto set = [this](dsp::Smoother& s, float t) { if (first) s.snap(t); else s.set(t); };
    set(wet, pv(P::RetroBypass) >= 0.5f ? 0.f : 1.f);
    set(mag, pv(P::RetroMagnitude) * 0.01f);
    set(noiseAmt, pv(P::RetroNoiseAmt) * 0.01f);
    set(tone, pv(P::RetroNoiseTone));
    set(fluxAmt, pv(P::RetroNoiseFlux) * 0.01f);
    set(wobAmt, pv(P::RetroWobbleAmt) * 0.01f);
    set(wobMix, pv(P::RetroWobbleMix) * 0.01f);
    set(distAmt, pv(P::RetroDistAmt) * 0.01f);
    set(focus, pv(P::RetroDistFocus));
    set(digiAmt, pv(P::RetroDigiAmt) * 0.01f);
    set(digiSmooth, pv(P::RetroDigiSmooth) * 0.01f);
    set(wearAmt, pv(P::RetroMagAmt) * 0.01f);
    set(dropAmt, pv(P::RetroMagDropouts) * 0.01f);
    noiseOn.set(pv(P::RetroNoiseOn) >= 0.5f ? 1.f : 0.f);
    wobOn.set(pv(P::RetroWobbleOn) >= 0.5f ? 1.f : 0.f);
    distOn.set(pv(P::RetroDistOn) >= 0.5f ? 1.f : 0.f);
    digiOn.set(pv(P::RetroDigiOn) >= 0.5f ? 1.f : 0.f);
    magOn.set(pv(P::RetroMagOn) >= 0.5f ? 1.f : 0.f);
    noiseType.request(juce::jlimit(0, 3, (int) pv(P::RetroNoiseType)));
    distType.request(juce::jlimit(0, 4, (int) pv(P::RetroDistType)));
    const bool wobStereo = pv(P::RetroWobbleStereo) >= 0.5f;
    const float levels = std::exp2(pv(P::RetroDigiBits) - 1.f);
    if (first) { updateFilters(true); first = false; }

    const float invFs = (float) (1.0 / fs), msToSamp = (float) (fs * 0.001);
    float* io[2] = { L, R };

    for (int i = 0; i < n; ++i)
    {
        const float w = wet.next();
        float dry[2];
        for (int c = 0; c < 2; ++c) { ch[c].dry.push(io[c][i]); dry[c] = ch[c].dry.readInt(baseDelay); }

        const bool anyOn = noiseOn.active() || wobOn.active() || distOn.active() || digiOn.active() || magOn.active();
        if (! anyOn || ! (w > 0.f || wet.isSmoothing())) // nothing to do: output = aligned dry
        {
            for (int c = 0; c < 2; ++c) { ch[c].wob.push(io[c][i]); io[c][i] = dry[c]; }
            continue;
        }

        updateFilters(false);
        const float m = mag.next(), na = noiseAmt.next(); tone.next();
        const float fx = fluxAmt.next() * (0.7f * flux1.next(rng, 0.6f, fs) + 0.3f * flux2.next(rng, 1.9f, fs)); // -1..1 scaled
        const float wa = wobAmt.next(), wm = wobMix.next(), da = distAmt.next(); focus.next();
        digiAmt.next(); digiSmooth.next();
        const float wear = wearAmt.next() * m, drop = dropAmt.next() * m;
        const float nOn = noiseOn.next(), wOn = wobOn.next(), dOn = distOn.next(), gOn = digiOn.next(), mOn = magOn.next();

        // --- shared oscillators (noise sources, wobble LFOs) ---
        humPh += 60.f * invFs; if (humPh >= 1.f) humPh -= 1.f;
        buzzPh += 59.94f * invFs; if (buzzPh >= 1.f) buzzPh -= 1.f;
        whinePh += 15734.f * invFs; if (whinePh >= 1.f) whinePh -= 1.f;
        const float wobRateMod = 1.f + 0.2f * fx;
        wobPh[0] += 1.4f * wobRateMod * invFs;
        wobPh[1] += (wobStereo ? 1.7f : 1.4f) * wobRateMod * invFs;
        for (auto& p : wobPh) if (p >= 1.f) p -= 1.f;

        // --- noise level, distortion drive (+ level compensation at -20 dBFS), dropouts ---
        const float nGateFade = noiseType.next();
        const float noiseGain = na * m * kNoiseLevel * kTypeGain[noiseType.current] * (1.f + 0.6f * fx) * nOn * nGateFade;
        const float dGateFade = distType.next();
        if (distOn.active() && --driveCounter <= 0) // drive moves slowly (flux): update every 16 samples
        {
            driveCounter = 16;
            drive = dsp::dbToGain(da * m * 27.f + 4.f * fx * (da > 0.f ? 1.f : 0.f));
            const float ref = 0.25f; // level-matched at -12 dBFS band peaks
            comp = ref * 2.f / std::max(1.0e-6f, shape(distType.current, ref * drive) - shape(distType.current, -ref * drive));
        }
        if (mOn > 0.f && dropRemaining <= 0 && rng.unipolar() < drop * 2.5f * invFs)
        {
            dropRemaining = (int) ((0.03f + 0.17f * rng.unipolar()) * (float) fs);
            dropTarget = dsp::dbToGain(-(6.f + 14.f * rng.unipolar()));
        }
        if (dropRemaining > 0 && --dropRemaining == 0) dropTarget = 1.f;
        dropGain = dropTarget + dropCoef * (dropGain - dropTarget);
        const float wearWobble = 1.f + 0.17f * wear * wearMod.next(rng, 8.f, fs);
        const float magGain = (1.f + (dropGain - 1.f) * mOn) * (1.f + (wearWobble - 1.f) * mOn);
        const float hum = (noiseOn.active() && noiseType.current == 3) ? noiseSample(0, 3, fx) : 0.f; // mono hum

        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            float x = io[c][i];

            // 1. Noise
            if (noiseOn.active())
            {
                const float src = noiseType.current == 3 ? hum : noiseSample(c, noiseType.current, fx);
                x += s.noise.toneLo.process(s.noise.toneHi.process(src)) * noiseGain;
            }

            // 2. Wobble (always runs the delay line so every path stays time-aligned)
            s.wob.push(x);
            const float aligned = s.wob.readInt(baseDelay);
            if (wobOn.active())
            {
                const float depthMs = std::min(1.9f, wa * m * kWobMaxMs * (1.f + 0.5f * fx));
                const float wobbled = s.wob.readFrac((float) baseDelay + dsp::fastSin01(wobPh[c]) * depthMs * msToSamp);
                x = aligned + (wobbled - aligned) * wm * wOn;
            }
            else x = aligned;

            // 3. Distort the Focus band
            if (distOn.active())
            {
                const float band = s.focus.process(x);
                const float shaped = shape(distType.current, band * drive) * comp;
                x += (s.dc.process(shaped) - band) * dOn * dGateFade;
            }

            // 4. Digital: sample-rate reduction + bit depth, then the Smooth low-pass
            if (digiOn.active())
            {
                s.digiPhase += digiInc;
                if (s.digiPhase >= 1.f)
                {
                    s.digiPhase -= std::floor(s.digiPhase);
                    s.held = std::round(dsp::clampf(x, -1.f, 1.f) * levels) / levels;
                }
                x += (s.digiLp.process(s.held) - x) * gOn;
            }

            // 5. Magnetic: wear (HF loss + soft compression + modulation noise) and smooth dropouts
            if (magOn.active())
            {
                const float k = 1.f + 2.f * wear;
                const float worn = dsp::fastTanh(s.wearLp.process(x) * k) / k;
                x = (x + (worn - x) * mOn) * magGain;
            }

            io[c][i] = dry[c] + (x - dry[c]) * w;
        }
    }
}
