#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <etlimit/LinearSmoother.h>

#include <cmath>

using etlimit::LinearSmoother;

TEST_CASE ("LinearSmoother reaches its target exactly after the ramp length", "[smoother]")
{
    LinearSmoother s;
    s.prepare (48000.0, 0.020);
    REQUIRE (s.getRampLength() == 960);

    s.setCurrentAndTarget (1.0);
    s.setTarget (0.123456789);

    double prev = 1.0;
    for (int i = 0; i < 959; ++i)
    {
        const double v = s.next();
        REQUIRE (v < prev); // strictly monotonic towards the target
        prev = v;
    }
    REQUIRE (s.isSmoothing());
    REQUIRE (s.next() == 0.123456789); // bit-exact at the end
    REQUIRE_FALSE (s.isSmoothing());
    REQUIRE (s.next() == 0.123456789);
}

TEST_CASE ("LinearSmoother steps are uniform (no jumps)", "[smoother]")
{
    LinearSmoother s;
    s.prepare (44100.0, 0.010);
    s.setCurrentAndTarget (0.0);
    s.setTarget (1.0);

    const double expectedStep = 1.0 / s.getRampLength();
    double prev = 0.0;
    for (int i = 0; i < s.getRampLength(); ++i)
    {
        const double v = s.next();
        REQUIRE_THAT (v - prev, Catch::Matchers::WithinAbs (expectedStep, 1e-12));
        prev = v;
    }
}

TEST_CASE ("LinearSmoother re-targeting mid-ramp is continuous", "[smoother]")
{
    LinearSmoother s;
    s.prepare (48000.0, 0.010);
    s.setCurrentAndTarget (0.0);
    s.setTarget (1.0);
    for (int i = 0; i < 100; ++i)
        s.next();

    const double before = s.getCurrent();
    s.setTarget (0.25);
    const double after = s.next();
    REQUIRE (std::abs (after - before) < 1.0 / s.getRampLength());
    for (int i = 1; i < s.getRampLength(); ++i)
        s.next();
    REQUIRE (s.getCurrent() == 0.25);
}

TEST_CASE ("LinearSmoother setting the same target does not restart a ramp", "[smoother]")
{
    LinearSmoother s;
    s.prepare (48000.0, 0.020);
    s.setCurrentAndTarget (0.5);
    s.setTarget (0.5);
    REQUIRE_FALSE (s.isSmoothing());
    REQUIRE (s.next() == 0.5);
}
