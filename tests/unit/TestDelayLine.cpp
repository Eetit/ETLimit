#include <catch2/catch_test_macros.hpp>

#include <etlimit/DelayLine.h>

#include <algorithm>
#include <cmath>
#include <vector>

using etlimit::DelayLine;

namespace
{
std::vector<float> runMono (DelayLine& d, std::vector<float> x, int block)
{
    for (size_t off = 0; off < x.size(); off += static_cast<size_t> (block))
    {
        float* p = x.data() + off;
        const int n = static_cast<int> (std::min (static_cast<size_t> (block), x.size() - off));
        d.process (&p, 1, n);
    }
    return x;
}
} // namespace

TEST_CASE ("DelayLine with zero delay is an exact identity", "[delay]")
{
    DelayLine d;
    d.prepare (1, 0);
    std::vector<float> x { 0.1f, -0.2f, 0.3f, 1.0f, -1.0f };
    REQUIRE (runMono (d, x, 2) == x);
}

TEST_CASE ("DelayLine delays an impulse by exactly N samples, across block boundaries", "[delay]")
{
    for (int delay : { 1, 7, 64, 255, 256 })
        for (int block : { 1, 3, 64, 1000 })
        {
            DelayLine d;
            d.prepare (1, 256);
            d.setDelay (delay);
            std::vector<float> x (2000, 0.0f);
            x[10] = 1.0f;
            const auto y = runMono (d, x, block);
            for (size_t i = 0; i < y.size(); ++i)
                REQUIRE (y[i] == (static_cast<int> (i) == 10 + delay ? 1.0f : 0.0f));
        }
}

TEST_CASE ("DelayLine keeps channels independent", "[delay]")
{
    DelayLine d;
    d.prepare (2, 16);
    d.setDelay (5);
    std::vector<float> l (40, 0.0f), r (40, 0.0f);
    l[0] = 1.0f;
    r[3] = -1.0f;
    float* ptrs[2] = { l.data(), r.data() };
    d.process (ptrs, 2, 40);
    REQUIRE (l[5] == 1.0f);
    REQUIRE (r[8] == -1.0f);
    float sumL = 0, sumR = 0;
    for (int i = 0; i < 40; ++i)
    {
        sumL += std::abs (l[static_cast<size_t> (i)]);
        sumR += std::abs (r[static_cast<size_t> (i)]);
    }
    REQUIRE (sumL == 1.0f);
    REQUIRE (sumR == 1.0f);
}

TEST_CASE ("DelayLine clamps delays beyond its capacity", "[delay]")
{
    DelayLine d;
    d.prepare (1, 10);
    d.setDelay (1000);
    REQUIRE (d.getDelay() == 10);
    d.setDelay (-5);
    REQUIRE (d.getDelay() == 0);
}

TEST_CASE ("DelayLine reset clears history", "[delay]")
{
    DelayLine d;
    d.prepare (1, 8);
    d.setDelay (4);
    std::vector<float> x (8, 1.0f);
    runMono (d, x, 8);
    d.reset();
    const auto y = runMono (d, std::vector<float> (4, 0.0f), 4);
    for (float v : y)
        REQUIRE (v == 0.0f);
}
