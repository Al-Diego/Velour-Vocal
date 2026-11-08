#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

/** One snapshot of what the processor is doing, sampled on a fixed set of
    log-spaced frequencies (independent of FFT size / sample rate). */
struct DisplayFrame
{
    static constexpr int   numPoints = 256;
    static constexpr float minHz     = 20.0f;
    static constexpr float maxHz     = 20000.0f;

    std::array<float, numPoints> reductionDb {}; // >= 0, how much each frequency is being cut
    std::array<float, numPoints> spectrumDb  {}; // input level, dBFS

    static float frequencyForPoint (float index) noexcept
    {
        return minHz * std::pow (maxHz / minHz, index / (float) (numPoints - 1));
    }

    static float pointForFrequency (float hz) noexcept
    {
        return (float) (numPoints - 1) * std::log (hz / minHz) / std::log (maxHz / minHz);
    }
};

/** Single-producer / single-consumer lock-free queue of DisplayFrames.
    The audio thread pushes (and silently drops a frame if the UI is behind),
    the message thread pulls the newest one. No allocation after construction. */
class DisplayFifo
{
public:
    void push (const DisplayFrame& frame) noexcept
    {
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)       frames[(size_t) scope.startIndex1] = frame;
        else if (scope.blockSize2 > 0)  frames[(size_t) scope.startIndex2] = frame;
    }

    /** Copies the most recent frame into dest. Returns false if nothing new arrived. */
    bool pullLatest (DisplayFrame& dest) noexcept
    {
        const int ready = fifo.getNumReady();
        if (ready <= 0)
            return false;

        const auto scope = fifo.read (ready);
        const int last = scope.blockSize2 > 0 ? scope.startIndex2 + scope.blockSize2 - 1
                                              : scope.startIndex1 + scope.blockSize1 - 1;
        dest = frames[(size_t) last];
        return true;
    }

private:
    static constexpr int capacity = 4;
    juce::AbstractFifo fifo { capacity };
    std::array<DisplayFrame, capacity> frames {};
};
