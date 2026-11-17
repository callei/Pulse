#pragma once
#include <juce_dsp/juce_dsp.h>
#include "../Params.h"
#include "../dsp/GlobalSection.h"
#include "../dsp/TapeModule.h"

// The complete Pulse signal path, independent of the plugin wrapper (also used by the offline renderer).
//   input gain -> [oversampled: TAPE -> GRIT -> SHAPE -> PUNCH] -> width -> auto-gain -> mix -> output gain
// Latency is constant for a given sample rate (Eco is padded to match High).
class PulseEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setSeed(uint32_t s) { seed = s; }
    // Processes 1 or 2 channels in place. Any block size.
    void process(juce::AudioBuffer<float>& buffer, const P::Values& v);
    int getLatencySamples() const { return latency; }

    GlobalSection global;

private:
    struct Chain
    {
        TapeModule tape;
        void prepare(double baseRate, int factor) { tape.prepare(baseRate, factor); }
        void reset(uint32_t s) { tape.reset(s); }
        void process(float* L, float* R, int n, const P::Values& v) { tape.process(L, R, n, v); }
    };
    void processChunk(float* L, float* R, int n, const P::Values& v);

    double fs = 48000.0;
    int maxBlock = 512;
    uint32_t seed = 0x5EED1234u;
    std::unique_ptr<juce::dsp::Oversampling<float>> os[2]; // [0] = Eco 2x, [1] = High 4x
    Chain chain[2];
    dsp::FracDelay align[2][2]; // [quality][channel]
    int latency = 0;
    int activeQuality = 1, pendingQuality = 1;
    bool firstBlock = true;
    float switchGain = 1.f, switchStep = 0.f;
    juce::AudioBuffer<float> scratch;
};
