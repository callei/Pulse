#pragma once
#include "DspUtils.h"

// Base-rate true-peak look-ahead limiter (8x true-peak estimate): catches the overs that remain after the oversampled
// Finish clipper. Gain = hold-min over the
// look-ahead window, then box-averaged (guarantees the ceiling), with a 50 ms release.
// Constant delay: delaySamples().
class TruePeakLimiter
{
public:
    static constexpr int kInterpHalf = 16, kInterpTaps = 32, kPhases = 8;

    static int lookaheadFor(double sr) { return std::max(16, (int) std::lround(0.0005 * sr)); }
    int delaySamples() const { return kInterpHalf - 1 + lookahead; }

    void prepare(double sr, float ceilingDb)
    {
        lookahead = lookaheadFor(sr);
        ceiling = dsp::dbToGain(ceilingDb);
        relCoef = dsp::onePoleCoef(50.f, sr);
        for (int p = 1; p < kPhases; ++p) // fractional positions 1/8 .. 7/8
        {
            const double frac = (double) p / kPhases;
            double sum = 0;
            for (int k = 0; k < kInterpTaps; ++k)
            {
                const double t = (k - (kInterpHalf - 1)) - frac, x = 3.141592653589793 * t;
                const double sinc = std::abs(t) < 1e-9 ? 1.0 : std::sin(x) / x;
                const double w = 0.5 + 0.5 * std::cos(3.141592653589793 * t / kInterpHalf);
                coef[p][k] = (float) (sinc * w);
                sum += coef[p][k];
            }
            for (auto& c : coef[p]) c = (float) (c / sum);
            double sa = 0; for (auto c : coef[p]) sa += std::abs(c);
            sumAbs = std::max(sumAbs, (float) sa);
        }
        for (auto& d : in) d.prepare(kInterpTaps + lookahead + 8);
        gainHist.prepare(lookahead * 2 + 8);
        reqHist.prepare(lookahead + 8);
        reset();
    }
    void reset()
    {
        for (auto& d : in) d.reset();
        gainHist.reset(); reqHist.reset();
        boxSum = lookahead; // history starts at gain 1
        for (int i = 0; i < lookahead + 4; ++i) { reqHist.push(1.f); gainHist.push(1.f); }
        released = 1.f;
    }
    // amount 0..1 crossfades the limiter in/out (delay is always applied)
    void process(float* L, float* R, int n, float amount)
    {
        for (int i = 0; i < n; ++i)
        {
            in[0].push(L[i]); in[1].push(R[i]);
            // true-peak estimate around the sample at delay kInterpHalf-1
            float tp = 0.f;
            if (amount <= 0.f) { reqHist.push(1.f); gainHist.push(1.f); boxSum += 1.0 - gainHist.readInt(lookahead); released = 1.f;
                                 L[i] = in[0].readInt(delaySamples()); R[i] = in[1].readInt(delaySamples()); continue; }
            float wmax = 0.f; // cheap bound: if even the worst-case interpolation can't reach the ceiling, skip it
            for (int c = 0; c < 2; ++c) for (int k = 0; k < kInterpTaps; ++k) wmax = std::max(wmax, std::abs(in[c].readInt(k)));
            if (wmax * sumAbs < ceiling) tp = wmax;
            else for (int c = 0; c < 2; ++c)
            {
                tp = std::max(tp, std::max(std::abs(in[c].readInt(kInterpHalf - 1)), std::abs(in[c].readInt(kInterpHalf))));
                for (int p = 1; p < kPhases; ++p)
                {
                    float s = 0.f;
                    for (int k = 0; k < kInterpTaps; ++k) s += coef[p][k] * in[c].readInt(kInterpTaps - 1 - k);
                    tp = std::max(tp, std::abs(s));
                }
            }
            const float req = tp > ceiling ? ceiling / tp : 1.f;
            reqHist.push(req);
            float held = 1.f; // min over the last 'lookahead' required gains
            for (int k = 0; k < lookahead; ++k) held = std::min(held, reqHist.readInt(k));
            gainHist.push(held);
            boxSum += held - gainHist.readInt(lookahead);
            const float box = (float) (boxSum / lookahead);
            released = std::min(box, 1.f + relCoef * (released - 1.f));
            const float g = 1.f + (released - 1.f) * amount;
            L[i] = in[0].readInt(delaySamples()) * g;
            R[i] = in[1].readInt(delaySamples()) * g;
        }
    }

private:
    int lookahead = 24;
    float ceiling = 0.944f, relCoef = 0.f, released = 1.f, sumAbs = 1.f;
    double boxSum = 0.0;
    float coef[kPhases][kInterpTaps] {};
    dsp::DelayLine in[2], gainHist, reqHist;
};
