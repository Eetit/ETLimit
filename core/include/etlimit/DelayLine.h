#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

namespace etlimit
{

/**
    Multi-channel integer delay (a "ring buffer").

    The limiter delays the audio by its lookahead so that the gain can start
    dropping *before* a peak arrives. The bypass path must be delayed by the
    exact same amount, otherwise toggling bypass would make the audio jump in
    time (a click) and the host's latency compensation would be wrong.

    Memory is allocated only in prepare(). setDelay() just moves a read
    position, so it is safe on the audio thread.
*/
class DelayLine
{
public:
    /** Allocate. NOT real-time safe: call from prepare only. */
    void prepare (int numChannels, int maxDelaySamples)
    {
        assert (numChannels > 0 && maxDelaySamples >= 0);
        maxDelay = maxDelaySamples;
        size = maxDelaySamples + 1;
        buffers.assign (static_cast<size_t> (numChannels), std::vector<float> (static_cast<size_t> (size), 0.0f));
        delay = std::min (delay, maxDelay);
        writePos = 0;
    }

    void reset() noexcept
    {
        for (auto& b : buffers)
            std::fill (b.begin(), b.end(), 0.0f);
        writePos = 0;
    }

    /** Change the delay. Real-time safe. Values are clamped to [0, maxDelay]. */
    void setDelay (int samples) noexcept { delay = std::clamp (samples, 0, maxDelay); }

    int getDelay() const noexcept { return delay; }
    int getMaxDelay() const noexcept { return maxDelay; }

    /** Delay numSamples of every channel in place. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        const int nch = std::min (numChannels, static_cast<int> (buffers.size()));
        int endPos = writePos;

        for (int ch = 0; ch < nch; ++ch)
        {
            float* buf = buffers[static_cast<size_t> (ch)].data();
            float* io = channels[ch];
            int w = writePos;

            for (int i = 0; i < numSamples; ++i)
            {
                buf[w] = io[i];
                int r = w - delay;
                if (r < 0)
                    r += size;
                io[i] = buf[r];
                if (++w == size)
                    w = 0;
            }
            endPos = w;
        }
        writePos = endPos;
    }

private:
    std::vector<std::vector<float>> buffers;
    int size = 1, maxDelay = 0, delay = 0, writePos = 0;
};

} // namespace etlimit
