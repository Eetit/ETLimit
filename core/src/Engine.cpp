#include "etlimit/Engine.h"

#include <algorithm>
#include <cmath>

namespace etlimit
{

void Engine::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    blockSize = std::max (1, maxBlockSize);
    preparedChannels = std::clamp (numChannels, 1, maxChannels);

    // Stage 1 adds no latency. Later stages compute this from lookahead and
    // oversampling filter delays.
    latencySamples = 0;

    inputGain.prepare (sampleRate, gainRampSeconds);
    wetMix.prepare (sampleRate, bypassRampSeconds);

    dryDelay.prepare (preparedChannels, latencySamples);
    dryDelay.setDelay (latencySamples);

    dryBuffers.assign (static_cast<size_t> (preparedChannels), std::vector<float> (static_cast<size_t> (blockSize), 0.0f));
    dryPointers.resize (static_cast<size_t> (preparedChannels));
    for (size_t ch = 0; ch < dryBuffers.size(); ++ch)
        dryPointers[ch] = dryBuffers[ch].data();

    nonFiniteInputs.store (0, std::memory_order_relaxed);
    reset();
}

void Engine::reset() noexcept
{
    dryDelay.reset();
    firstBlock = true;
}

void Engine::setSettings (const EngineSettings& newSettings) noexcept
{
    settings = newSettings;

    const double gainTarget = dbToGain (static_cast<double> (settings.inputGainDb));
    const double mixTarget = settings.bypass ? 0.0 : 1.0;

    if (firstBlock)
    {
        // Right after prepare/reset there is no previous audio to stay
        // continuous with, so start directly at the requested values.
        inputGain.setCurrentAndTarget (gainTarget);
        wetMix.setCurrentAndTarget (mixTarget);
        firstBlock = false;
    }
    else
    {
        inputGain.setTarget (gainTarget);
        wetMix.setTarget (mixTarget);
    }
}

void Engine::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (numSamples <= 0 || numChannels <= 0 || channels == nullptr)
        return;

    if (firstBlock)
        setSettings (settings);

    // Channels beyond what we prepared for cannot be processed safely
    // without allocating. Output silence on them rather than garbage.
    for (int ch = preparedChannels; ch < numChannels; ++ch)
        std::fill (channels[ch], channels[ch] + numSamples, 0.0f);

    const int nch = std::min (numChannels, preparedChannels);

    // Hosts occasionally send bigger blocks than announced. Process them in
    // chunks that fit our pre-allocated buffers instead of failing.
    float* chunk[maxChannels] = {};
    for (int offset = 0; offset < numSamples; offset += blockSize)
    {
        const int n = std::min (blockSize, numSamples - offset);
        for (int ch = 0; ch < nch; ++ch)
            chunk[ch] = channels[ch] + offset;
        processChunk (chunk, nch, n);
    }
}

void Engine::processChunk (float* const* channels, int nch, int n) noexcept
{
    // 1. Sanitise. A single NaN or infinity would otherwise poison every
    //    filter and envelope downstream, permanently. We replace them with
    //    silence and count the event for diagnostics. Absurdly large (but
    //    finite) values are clamped so later gain can't overflow.
    std::uint64_t badCount = 0;
    for (int ch = 0; ch < nch; ++ch)
    {
        float* x = channels[ch];
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (x[i]))
            {
                x[i] = 0.0f;
                ++badCount;
            }
            else
            {
                x[i] = std::clamp (x[i], -maxSampleMagnitude, maxSampleMagnitude);
            }
        }
    }
    if (badCount != 0)
        nonFiniteInputs.fetch_add (badCount, std::memory_order_relaxed);

    // 2. Keep a copy of the input for the bypass path, delayed by exactly
    //    the engine latency so that wet and dry stay time-aligned.
    for (int ch = 0; ch < nch; ++ch)
        std::copy (channels[ch], channels[ch] + n, dryPointers[static_cast<size_t> (ch)]);
    dryDelay.process (dryPointers.data(), nch, n);

    // 3. Input gain. Computed in double, applied to float samples.
    //    (Later stages: limiter, oversampling, true-peak guard go here.)
    if (! inputGain.isSmoothing() && inputGain.getCurrent() == 1.0)
    {
        // Unity gain: leave the samples untouched (bit-exact).
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const double g = inputGain.next();
            for (int ch = 0; ch < nch; ++ch)
            {
                const double y = static_cast<double> (channels[ch][i]) * g;
                channels[ch][i] = static_cast<float> (std::clamp (y, -static_cast<double> (maxSampleMagnitude),
                                                                   static_cast<double> (maxSampleMagnitude)));
            }
        }
    }

    // 4. Bypass crossfade. Wet and dry are the same signal shifted by the
    //    same latency, so a linear (equal-gain) crossfade is click-free.
    if (! wetMix.isSmoothing())
    {
        if (wetMix.getCurrent() == 0.0)
            for (int ch = 0; ch < nch; ++ch)
                std::copy (dryPointers[static_cast<size_t> (ch)], dryPointers[static_cast<size_t> (ch)] + n, channels[ch]);
        // wetMix == 1: output is already the wet signal.
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const double w = wetMix.next();
            for (int ch = 0; ch < nch; ++ch)
            {
                const double wet = channels[ch][i];
                const double dry = dryPointers[static_cast<size_t> (ch)][i];
                channels[ch][i] = static_cast<float> (dry + w * (wet - dry));
            }
        }
    }
}

} // namespace etlimit
