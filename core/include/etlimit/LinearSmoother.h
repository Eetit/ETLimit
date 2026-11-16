#pragma once

#include <algorithm>
#include <cmath>

namespace etlimit
{

/** Exact floating-point comparison, on purpose. Used only where we need to
    know whether a value is *identical* (for example: "is this the same
    target as before?"), not "approximately equal". */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
inline bool exactlyEqual (double a, double b) noexcept { return a == b; }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

/**
    Moves a value (for example a gain) towards a target in a straight line
    over a fixed number of samples.

    Why: if a gain jumped instantly from one value to another, the waveform
    would get a sudden step, which you hear as a click. Spreading the change
    over ~20 ms makes it inaudible.

    Why linear and not exponential: a linear ramp reaches the target exactly,
    in a known number of samples. When the ramp is finished, next() returns
    the target value bit-for-bit, so a 0 dB gain is a true multiplication by
    exactly 1.0 (important for the bit-exact pass-through test).

    Real-time safe: no allocation, no locks.
*/
class LinearSmoother
{
public:
    /** Call from prepare(). rampSeconds is the duration of one full ramp. */
    void prepare (double sampleRate, double rampSeconds) noexcept
    {
        rampLength = std::max (1, static_cast<int> (std::lround (sampleRate * rampSeconds)));
    }

    /** Jump immediately (used on reset / first block, never while audio plays). */
    void setCurrentAndTarget (double value) noexcept
    {
        current = target = value;
        increment = 0.0;
        remaining = 0;
    }

    /** Start a new ramp from wherever we are now to newTarget. */
    void setTarget (double newTarget) noexcept
    {
        if (exactlyEqual (newTarget, target))
            return;

        target = newTarget;
        remaining = rampLength;
        increment = (target - current) / static_cast<double> (rampLength);
    }

    /** Advance one sample and return the new value. */
    double next() noexcept
    {
        if (remaining > 0)
        {
            --remaining;
            current = (remaining == 0) ? target : current + increment;
        }
        return current;
    }

    bool isSmoothing() const noexcept { return remaining > 0; }
    double getCurrent() const noexcept { return current; }
    double getTarget() const noexcept { return target; }
    int getRampLength() const noexcept { return rampLength; }

private:
    double current = 0.0, target = 0.0, increment = 0.0;
    int remaining = 0;
    int rampLength = 1;
};

} // namespace etlimit
