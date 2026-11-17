#include "GlobalSection.h"

void GlobalSection::prepare(double sampleRate, int dryDelaySamples, int maxBlock)
{
    fs = sampleRate;
    dryDelay = dryDelaySamples;
    for (auto& d : dry) d.prepare(dryDelay + maxBlock + 8);
    for (auto* s : { &inGain, &outGain, &width, &mix }) s->prepare(fs, 30.f);
    agCoef = dsp::onePoleCoef(1000.f, fs);  // 1 s loudness integration
    agSmooth = dsp::onePoleCoef(300.f, fs); // gain glide
    reset();
}

void GlobalSection::reset()
{
    for (auto& d : dry) d.reset();
    pDry = pWet = 0.f;
    agGain = 1.f;
    first = true;
}

static void updatePeak(std::atomic<float>& a, float v)
{
    float cur = a.load(std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {}
}

void GlobalSection::pre(float* L, float* R, int n, const P::Values& v)
{
    if (first)
    {
        inGain.snap(dsp::dbToGain(v[P::InputGain])); outGain.snap(dsp::dbToGain(v[P::OutputGain]));
        width.snap(v[P::MasterWidth] * 0.01f); mix.snap(v[P::MasterMix] * 0.01f);
        first = false;
    }
    inGain.set(dsp::dbToGain(v[P::InputGain]));
    float pk[2] {};
    for (int i = 0; i < n; ++i)
    {
        pk[0] = std::max(pk[0], std::abs(L[i])); pk[1] = std::max(pk[1], std::abs(R[i]));
        const float g = inGain.next();
        L[i] *= g; R[i] *= g;
        dry[0].push(L[i]); dry[1].push(R[i]);
    }
    updatePeak(inPeak[0], pk[0]); updatePeak(inPeak[1], pk[1]);
}

void GlobalSection::post(float* L, float* R, int n, const P::Values& v)
{
    outGain.set(dsp::dbToGain(v[P::OutputGain]));
    width.set(v[P::MasterWidth] * 0.01f);
    mix.set(v[P::MasterMix] * 0.01f);
    const bool autoGain = v[P::AutoGain] >= 0.5f;
    const float gate = 1.0e-6f; // -60 dBFS power: below this, hold the auto-gain value
    float pk[2] {};

    // dry[] was pushed n samples ahead in pre(); read it back aligned with the processed signal.
    for (int i = 0; i < n; ++i)
    {
        const float dL = dry[0].readInt(dryDelay + (n - 1 - i));
        const float dR = dry[1].readInt(dryDelay + (n - 1 - i));

        const float w = width.next();
        const float m = (L[i] + R[i]) * 0.5f, s = (L[i] - R[i]) * 0.5f * w;
        float wl = m + s, wr = m - s;

        if (autoGain)
        {
            pDry = (dL * dL + dR * dR) * 0.5f + agCoef * (pDry - (dL * dL + dR * dR) * 0.5f);
            pWet = (wl * wl + wr * wr) * 0.5f + agCoef * (pWet - (wl * wl + wr * wr) * 0.5f);
            if (pDry > gate && pWet > gate * 0.01f)
            {
                const float target = dsp::clampf(std::sqrt(pDry / pWet), 0.125f, 8.f); // +/-18 dB
                agGain = target + agSmooth * (agGain - target);
            }
        }
        else
        {
            agGain = 1.f + agSmooth * (agGain - 1.f);
        }
        wl *= agGain; wr *= agGain;

        const float mx = mix.next(), og = outGain.next();
        L[i] = (dL + (wl - dL) * mx) * og;
        R[i] = (dR + (wr - dR) * mx) * og;
        pk[0] = std::max(pk[0], std::abs(L[i])); pk[1] = std::max(pk[1], std::abs(R[i]));
    }
    updatePeak(outPeak[0], pk[0]); updatePeak(outPeak[1], pk[1]);
    autoGainDb.store(dsp::gainToDb(agGain), std::memory_order_relaxed);
}
