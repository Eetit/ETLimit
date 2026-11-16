#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <etlimit/Engine.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

using etlimit::Engine;
using etlimit::EngineSettings;

namespace
{
using Buffer = std::vector<std::vector<float>>; // [channel][sample]

Buffer makeNoise (int channels, int samples, unsigned seed, float amplitude = 1.0f)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-amplitude, amplitude);
    Buffer b (static_cast<size_t> (channels), std::vector<float> (static_cast<size_t> (samples)));
    for (auto& ch : b)
        for (auto& v : ch)
            v = dist (rng);
    return b;
}

/** Process `b` through `e` in blocks of `block` samples (0 = random sizes 1..2048). */
void run (Engine& e, Buffer& b, int block, const EngineSettings& s, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_int_distribution<int> dist (1, 2048);
    const int total = static_cast<int> (b[0].size());
    std::vector<float*> ptrs (b.size());
    for (int pos = 0; pos < total;)
    {
        const int n = std::min (block > 0 ? block : dist (rng), total - pos);
        for (size_t ch = 0; ch < b.size(); ++ch)
            ptrs[ch] = b[ch].data() + pos;
        e.setSettings (s);
        e.process (ptrs.data(), static_cast<int> (b.size()), n);
        pos += n;
    }
}
} // namespace

TEST_CASE ("Engine at 0 dB is bit-exact pass-through for every block size, rate and channel count", "[engine]")
{
    const double rate = GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 192000.0);
    const int channels = GENERATE (1, 2);
    const int block = GENERATE (0, 1, 32, 64, 441, 2048, 4096); // 4096 > announced max, 0 = random

    Engine e;
    e.prepare (rate, 2048, channels);
    REQUIRE (e.getLatencySamples() == 0);

    const Buffer input = makeNoise (channels, 20000, 42);
    Buffer b = input;
    run (e, b, block, {});
    REQUIRE (b == input);
}

TEST_CASE ("Engine input gain is accurate once settled", "[engine]")
{
    const float gainDb = GENERATE (-12.0f, -6.0f, 0.5f, 6.0f, 20.0f, 36.0f);
    Engine e;
    e.prepare (48000.0, 512, 2);

    EngineSettings s;
    s.inputGainDb = gainDb; // set before the first block: no ramp
    const Buffer input = makeNoise (2, 4096, 7, 0.01f);
    Buffer b = input;
    run (e, b, 512, s);

    const double g = etlimit::dbToGain (gainDb);
    for (size_t ch = 0; ch < 2; ++ch)
        for (size_t i = 0; i < b[ch].size(); ++i)
            REQUIRE (b[ch][i] == static_cast<float> (static_cast<double> (input[ch][i]) * g));
}

TEST_CASE ("Engine gain changes ramp smoothly over the documented time", "[engine]")
{
    const double rate = 48000.0;
    Engine e;
    e.prepare (rate, 256, 1);

    // Constant input makes the applied gain directly visible in the output.
    Buffer b (1, std::vector<float> (8192, 0.25f));
    std::vector<float*> ptrs { nullptr };
    EngineSettings s;

    int pos = 0;
    auto block = [&] (int n) {
        ptrs[0] = b[0].data() + pos;
        e.setSettings (s);
        e.process (ptrs.data(), 1, n);
        pos += n;
    };

    block (1024);               // settle at 0 dB
    s.inputGainDb = 12.0f;      // step change
    for (int i = 0; i < 28; ++i)
        block (256);

    const int ramp = static_cast<int> (std::lround (rate * Engine::gainRampSeconds));
    const double g1 = etlimit::dbToGain (12.0);
    const double maxStep = 0.25 * (g1 - 1.0) / ramp;

    for (int i = 1024; i < 1024 + ramp; ++i)
    {
        const double step = static_cast<double> (b[0][static_cast<size_t> (i)]) - b[0][static_cast<size_t> (i - 1)];
        REQUIRE (step > 0.0);                 // monotonic
        REQUIRE (step <= maxStep * 1.0001);   // no jumps bigger than one ramp step
    }
    REQUIRE (b[0][static_cast<size_t> (1024 + ramp - 1)] == static_cast<float> (0.25 * g1));
    REQUIRE (b[0].back() == static_cast<float> (0.25 * g1));
}

TEST_CASE ("Engine replaces NaN and infinity with silence and counts them", "[engine][safety]")
{
    Engine e;
    e.prepare (44100.0, 64, 2);
    Buffer b = makeNoise (2, 256, 3, 0.5f);
    const Buffer original = b;
    b[0][10] = std::numeric_limits<float>::quiet_NaN();
    b[1][20] = std::numeric_limits<float>::infinity();
    b[0][200] = -std::numeric_limits<float>::infinity();

    run (e, b, 64, {});

    REQUIRE (e.getNonFiniteInputCount() == 3);
    REQUIRE (b[0][10] == 0.0f);
    REQUIRE (b[1][20] == 0.0f);
    REQUIRE (b[0][200] == 0.0f);
    for (size_t ch = 0; ch < 2; ++ch)
        for (size_t i = 0; i < b[ch].size(); ++i)
        {
            REQUIRE (std::isfinite (b[ch][i]));
            if (! ((ch == 0 && (i == 10 || i == 200)) || (ch == 1 && i == 20)))
                REQUIRE (b[ch][i] == original[ch][i]); // neighbours untouched
        }

    // The engine keeps working normally afterwards.
    Buffer clean = makeNoise (2, 512, 4);
    const Buffer cleanCopy = clean;
    run (e, clean, 64, {});
    REQUIRE (clean == cleanCopy);
}

TEST_CASE ("Engine survives extreme input levels without overflow", "[engine][safety]")
{
    Engine e;
    e.prepare (48000.0, 128, 1);
    Buffer b (1, std::vector<float> (1024));
    for (size_t i = 0; i < b[0].size(); ++i)
        b[0][i] = (i % 2 == 0) ? std::numeric_limits<float>::max() : -std::numeric_limits<float>::max();

    EngineSettings s;
    s.inputGainDb = 36.0f;
    run (e, b, 128, s);
    for (float v : b[0])
    {
        REQUIRE (std::isfinite (v));
        REQUIRE (std::abs (v) <= Engine::maxSampleMagnitude);
    }
}

TEST_CASE ("Engine keeps digital silence silent", "[engine]")
{
    Engine e;
    e.prepare (96000.0, 32, 2);
    Buffer b (2, std::vector<float> (10000, 0.0f));
    EngineSettings s;
    s.inputGainDb = 36.0f;
    run (e, b, 32, s);
    for (auto& ch : b)
        for (float v : ch)
            REQUIRE (v == 0.0f);
}

TEST_CASE ("Engine bypass outputs the exact dry signal and crossfades without jumps", "[engine][bypass]")
{
    const double rate = 44100.0;
    Engine e;
    e.prepare (rate, 512, 1);

    // A smooth sine so that any discontinuity would stand out.
    const int total = 16384;
    Buffer input (1, std::vector<float> (static_cast<size_t> (total)));
    for (int i = 0; i < total; ++i)
        input[0][static_cast<size_t> (i)] = static_cast<float> (0.5 * std::sin (2.0 * 3.141592653589793 * 1000.0 * i / rate));

    Buffer b = input;
    std::vector<float*> ptrs { nullptr };
    EngineSettings s;
    s.inputGainDb = 6.0f;
    int pos = 0;
    auto block = [&] (int n) {
        ptrs[0] = b[0].data() + pos;
        e.setSettings (s);
        e.process (ptrs.data(), 1, n);
        pos += n;
    };

    block (4096);
    s.bypass = true;
    block (4096);
    block (4096);
    s.bypass = false;
    block (4096);

    const int ramp = static_cast<int> (std::lround (rate * Engine::bypassRampSeconds));

    // After the bypass crossfade has completed: exactly the dry input.
    for (int i = 4096 + ramp; i < 3 * 4096; ++i)
        REQUIRE (b[0][static_cast<size_t> (i)] == input[0][static_cast<size_t> (i)]);

    // No sample-to-sample jump anywhere larger than the steepest slope of the
    // (gained) sine itself plus the crossfade contribution.
    const double g = etlimit::dbToGain (6.0);
    const double sineSlope = 0.5 * g * 2.0 * 3.141592653589793 * 1000.0 / rate;
    const double fadeSlope = 0.5 * (g - 1.0) / ramp;
    double maxJump = 0.0;
    for (int i = 1; i < total; ++i)
        maxJump = std::max (maxJump, std::abs (static_cast<double> (b[0][static_cast<size_t> (i)]) - b[0][static_cast<size_t> (i - 1)]));
    REQUIRE (maxJump <= (sineSlope + fadeSlope) * 1.001);
}

TEST_CASE ("Engine silences channels it was not prepared for", "[engine][safety]")
{
    Engine e;
    e.prepare (48000.0, 64, 1);
    Buffer b = makeNoise (2, 64, 9);
    const Buffer original = b;
    run (e, b, 64, {});
    REQUIRE (b[0] == original[0]);
    for (float v : b[1])
        REQUIRE (v == 0.0f);
}

TEST_CASE ("Engine handles zero-length and null blocks", "[engine][safety]")
{
    Engine e;
    e.prepare (48000.0, 64, 2);
    float* none[2] = { nullptr, nullptr };
    e.process (none, 2, 0);
    e.process (nullptr, 2, 64);
    SUCCEED();
}
