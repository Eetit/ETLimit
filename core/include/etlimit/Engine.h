#pragma once

#include "DelayLine.h"
#include "LinearSmoother.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace etlimit
{

/** Decibels to linear amplitude: 0 dB = 1.0, -6.02 dB = 0.5. */
inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }

/** Linear amplitude to decibels. */
inline double gainToDb (double gain) noexcept { return 20.0 * std::log10 (gain); }

/** All user-facing settings the engine understands at this stage.
    New fields are added stage by stage, only once they actually work. */
struct EngineSettings
{
    float inputGainDb = 0.0f;
    bool bypass = false;
};

/**
    The complete audio engine, independent of JUCE.

    Signal flow (stage 1):

        in --> sanitise (NaN/Inf -> 0) --+--> input gain --------------+--> bypass crossfade --> out
                                         |                             |
                                         +--> latency-matched dry ---->+

    Threading: prepare()/reset() run on a non-audio thread while audio is
    stopped. setSettings() and process() run on the audio thread and never
    allocate, lock or block.
*/
class Engine
{
public:
    static constexpr int maxChannels = 2;

    /** Allocates everything. maxBlockSize is a hint: larger host blocks are
        processed in internal chunks, so they are still handled safely. */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    /** Clear all internal state (delay lines, smoothers jump to targets). */
    void reset() noexcept;

    /** Call at the start of each block from the audio thread. */
    void setSettings (const EngineSettings& newSettings) noexcept;

    /** Process audio in place. numChannels may be 1 or 2. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Total delay the engine adds, in samples at the host sample rate. */
    int getLatencySamples() const noexcept { return latencySamples; }

    /** Number of NaN/Inf input samples replaced by silence since prepare().
        Safe to read from any thread. */
    std::uint64_t getNonFiniteInputCount() const noexcept { return nonFiniteInputs.load (std::memory_order_relaxed); }

    double getSampleRate() const noexcept { return sampleRate; }

    /** Largest sample magnitude the engine will carry (+180 dBFS). Anything
        larger is clamped. No real audio comes near this; it only exists so
        that absurd input times a large gain can never overflow to infinity. */
    static constexpr float maxSampleMagnitude = 1.0e9f;

    /** Ramp times, exposed so tests can reason about them. */
    static constexpr double gainRampSeconds = 0.020;
    static constexpr double bypassRampSeconds = 0.010;

private:
    void processChunk (float* const* channels, int numChannels, int numSamples) noexcept;

    double sampleRate = 44100.0;
    int blockSize = 0;
    int preparedChannels = 0;
    int latencySamples = 0;
    bool firstBlock = true;

    EngineSettings settings;
    LinearSmoother inputGain;
    LinearSmoother wetMix; // 1 = processed, 0 = bypassed

    DelayLine dryDelay;
    std::vector<std::vector<float>> dryBuffers;
    std::vector<float*> dryPointers;

    std::atomic<std::uint64_t> nonFiniteInputs { 0 };
};

} // namespace etlimit
