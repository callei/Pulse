#include "PulseEngine.h"

void PulseEngine::prepare(double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    maxBlock = std::max(1, maxBlockSize);
    using OS = juce::dsp::Oversampling<float>;
    double osLat[2];
    for (int q = 0; q < 2; ++q)
    {
        os[q] = std::make_unique<OS>(2, (size_t) (q + 1), OS::filterHalfBandFIREquiripple, false, false);
        os[q]->initProcessing((size_t) maxBlock);
        osLat[q] = os[q]->getLatencyInSamples();
        chain[q].prepare(fs, q == 0 ? 2 : 4, &telemetry);
    }
    const int wetLatency = (int) std::ceil(std::max(osLat[0], osLat[1])) + dsp::FracDelay::kCentre + 1;
    for (int q = 0; q < 2; ++q)
        for (auto& a : align[q]) a.prepare(wetLatency - osLat[q]);

    limiter.prepare(fs, -0.5f);
    limiterAmount.prepare(fs, 30.f);
    latency = wetLatency + Chain::fixedDelay(fs) + limiter.delaySamples();
    global.prepare(fs, latency, maxBlock);
    scratch.setSize(2, maxBlock, false, true, false);
    switchStep = 1.f / (float) std::max(1, (int) (0.005 * fs));
    reset();
}

void PulseEngine::reset()
{
    for (int q = 0; q < 2; ++q) { os[q]->reset(); chain[q].reset(seed); for (auto& a : align[q]) a.reset(); }
    global.reset();
    limiter.reset();
    limiterAmount.snap(1.f);
    switchGain = 1.f;
    firstBlock = true;
}

void PulseEngine::process(juce::AudioBuffer<float>& buffer, const P::Values& v)
{
    const int numCh = buffer.getNumChannels(), total = buffer.getNumSamples();
    if (numCh == 0 || total == 0) return;
    pendingQuality = v[P::Quality] >= 0.5f ? 1 : 0;
    if (firstBlock) { activeQuality = pendingQuality; switchGain = 1.f; firstBlock = false; }

    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = std::min(maxBlock, total - start);
        float* L = scratch.getWritePointer(0);
        float* R = scratch.getWritePointer(1);
        std::copy_n(buffer.getReadPointer(0, start), n, L);
        std::copy_n(buffer.getReadPointer(numCh > 1 ? 1 : 0, start), n, R);
        processChunk(L, R, n, v);
        std::copy_n(L, n, buffer.getWritePointer(0, start));
        if (numCh > 1) std::copy_n(R, n, buffer.getWritePointer(1, start));
    }
}

void PulseEngine::processChunk(float* L, float* R, int n, const P::Values& v)
{
    global.pre(L, R, n, v);

    float* chans[2] = { L, R };
    juce::dsp::AudioBlock<float> block(chans, 2, (size_t) n);
    auto& o = *os[activeQuality];
    auto up = o.processSamplesUp(block);
    chain[activeQuality].process(up.getChannelPointer(0), up.getChannelPointer(1), (int) up.getNumSamples(), v);
    o.processSamplesDown(block);

    // Round the oversampler's fractional latency up to the same whole number of samples for both qualities.
    for (int c = 0; c < 2; ++c) align[activeQuality][c].process(chans[c], n);

    // True-peak safety for the PUNCH Finish stage (delay always applied so latency stays constant).
    limiterAmount.set(v[P::FxBypass] < 0.5f && v[P::FxMoreOn] >= 0.5f ? 1.f : 0.f);
    float amt = limiterAmount.current;
    for (int i = 0; i < n; ++i) amt = limiterAmount.next();
    limiter.process(L, R, n, amt);

    // Quality switch: fade the processed path out (5 ms), swap oversampler/chain, fade back in.
    const bool switching = pendingQuality != activeQuality;
    if (switching || switchGain < 1.f)
    {
        for (int i = 0; i < n; ++i)
        {
            switchGain = switching ? std::max(0.f, switchGain - switchStep) : std::min(1.f, switchGain + switchStep);
            L[i] *= switchGain; R[i] *= switchGain;
        }
        if (switching && switchGain <= 0.f)
        {
            activeQuality = pendingQuality;
            os[activeQuality]->reset();
            chain[activeQuality].reset(seed);
            for (auto& a : align[activeQuality]) a.reset();
            switchGain = 0.f;
        }
    }

    global.post(L, R, n, v);
}
