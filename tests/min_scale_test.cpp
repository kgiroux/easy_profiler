#include "min_scale.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

using profiler_gui::computeMinScale;

namespace {

// Same constants as profiler_gui (globals.h / blocks_graphics_view.cpp)
const double kCoeff     = 1.25;
const double kCoeffInv  = 1.0 / kCoeff;
const double kMinScale  = std::pow(kCoeffInv, 60);
const double kMaxScale  = std::pow(kCoeff, 45);

double expectedScale(double durationNs)
{
    return std::pow(kCoeffInv, 10.0 * std::log10(durationNs * 1.10 / 1e9) + 30.0);
}

} // namespace

// Regression: a capture with only zero-duration blocks (EASY_EVENT) produced log(0) = -inf
// and therefore an infinite minimal scale.
TEST(ComputeMinScale, ZeroDurationFallsBackToDefault)
{
    const double scale = computeMinScale(0, kCoeffInv, kMinScale, kMaxScale);
    EXPECT_TRUE(std::isfinite(scale));
    EXPECT_DOUBLE_EQ(scale, kMinScale);
}

// Regression: durations below ~20 ns gave a minimal scale greater than the maximal scale.
TEST(ComputeMinScale, VeryShortDurationIsClampedToMaxScale)
{
    for (const std::uint64_t ns : {1ULL, 5ULL, 10ULL})
    {
        const double scale = computeMinScale(ns, kCoeffInv, kMinScale, kMaxScale);
        EXPECT_TRUE(std::isfinite(scale)) << ns;
        EXPECT_LE(scale, kMaxScale) << ns;
    }
}

TEST(ComputeMinScale, NominalDurationsKeepOriginalFormula)
{
    for (const std::uint64_t ns : {1'000ULL, 1'000'000ULL, 1'000'000'000ULL})
    {
        EXPECT_NEAR(computeMinScale(ns, kCoeffInv, kMinScale, kMaxScale), expectedScale(static_cast<double>(ns)),
                    expectedScale(static_cast<double>(ns)) * 1e-12) << ns;
    }
}

TEST(ComputeMinScale, LongerCaptureAllowsMoreZoomOut)
{
    const double oneSecond = computeMinScale(1'000'000'000ULL, kCoeffInv, kMinScale, kMaxScale);
    const double tenSeconds = computeMinScale(10'000'000'000ULL, kCoeffInv, kMinScale, kMaxScale);
    EXPECT_LT(tenSeconds, oneSecond);
    EXPECT_GT(tenSeconds, 0.0);
}
