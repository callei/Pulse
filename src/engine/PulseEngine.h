#pragma once
#include <juce_dsp/juce_dsp.h>
#include "../Params.h"
#include "../dsp/GlobalSection.h"
#include "../dsp/TapeModule.h"
#include "../dsp/GritModule.h"
#include "../dsp/ShapeModule.h"
#include "../dsp/PunchModule.h"
#include "../dsp/TruePeakLimiter.h"
#include "Telemetry.h"

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
    Telemetry telemetry;

private:
    struct Chain
    {
        TapeModule tape;
        GritModule grit;
        ShapeModule shape;
        PunchModule punch;
        void prepare(double baseRate, int factor, Telemetry* t)
        {
            tape.prepare(baseRate, factor); grit.prepare(baseRate, factor);
            shape.prepare(baseRate, factor); punch.prepare(baseRate, factor);
            shape.setTelemetry(t); punch.setTelemetry(t);
        }
        void reset(uint32_t s) { tape.reset(s); grit.reset(s); shape.reset(); punch.reset(); }
        void process(float* L, float* R, int n, const P::Values& v)
        {
            tape.process(L, R, n, v); grit.process(L, R, n, v); shape.process(L, R, n, v); punch.process(L, R, n, v);
        }
        // Constant delays of the modules (base-rate samples): tape modulation, wobble headroom, look-ahead.
        static int fixedDelay(double baseRate)
        {
            return TapeModule::baseDelayBaseRate(baseRate) + GritModule::baseDelayBaseRate(baseRate) + ShapeModule::lookaheadBaseRate(baseRate);
        }
    };
    void processChunk(float* L, float* R, int n, const P::Values& v);

    double fs = 48000.0;
    int maxBlock = 512;
    uint32_t seed = 0x5EED1234u;
    std::unique_ptr<juce::dsp::Oversampling<float>> os[2]; // [0] = Eco 2x, [1] = High 4x
    Chain chain[2];
    dsp::FracDelay align[2][2]; // [quality][channel]
    TruePeakLimiter limiter;
    dsp::Smoother limiterAmount;
    int latency = 0;
    int activeQuality = 1, pendingQuality = 1;
    bool firstBlock = true;
    float switchGain = 1.f, switchStep = 0.f;
    juce::AudioBuffer<float> scratch;
};
