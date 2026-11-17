#pragma once
#include <juce_core/juce_core.h>
#include <atomic>

// Audio thread -> UI, lock-free. The SHAPE gain trace is pushed at ~200 Hz; Boom triggers are counted.
struct Telemetry
{
    static constexpr int kSize = 2048;
    juce::AbstractFifo fifo { kSize };
    float data[kSize] {};
    std::atomic<int> boomCount { 0 };

    void pushShapeGain(float db) // audio thread
    {
        const auto w = fifo.write(1);
        if (w.blockSize1 > 0) data[w.startIndex1] = db;
    }
    int pull(float* dest, int maxCount) // UI thread
    {
        const auto r = fifo.read(std::min(maxCount, fifo.getNumReady()));
        for (int i = 0; i < r.blockSize1; ++i) dest[i] = data[r.startIndex1 + i];
        for (int i = 0; i < r.blockSize2; ++i) dest[r.blockSize1 + i] = data[r.startIndex2 + i];
        return r.blockSize1 + r.blockSize2;
    }
};
