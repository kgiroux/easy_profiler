#include "tick_conversion.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>

#ifdef _WIN32
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <Windows.h>
#endif

using profiler::detail::scale_ticks;

namespace {

constexpr std::uint64_t kNanoPerSec  = 1'000'000'000ULL;
constexpr std::uint64_t kMicroPerSec = 1'000'000ULL;
constexpr std::uint64_t kQpcFreq     = 10'000'000ULL; // typical QPC frequency on Windows 10+

} // namespace

// --- Compile-time checks: the helper must stay usable in constant expressions ---
static_assert(scale_ticks(0, kNanoPerSec, kQpcFreq) == 0);
static_assert(scale_ticks(kQpcFreq, kNanoPerSec, kQpcFreq) == kNanoPerSec);
static_assert(scale_ticks(123, 7, 0) == 0, "division by zero must be guarded");

TEST(ScaleTicks, IdentityWhenMultiplierEqualsDivisor)
{
    EXPECT_EQ(scale_ticks(42, kNanoPerSec, kNanoPerSec), 42U);
    EXPECT_EQ(scale_ticks(std::numeric_limits<std::uint64_t>::max(), 1, 1),
              std::numeric_limits<std::uint64_t>::max());
}

TEST(ScaleTicks, ZeroDivisorReturnsZero)
{
    EXPECT_EQ(scale_ticks(1'000, kNanoPerSec, 0), 0U);
}

TEST(ScaleTicks, ConvertsQpcTicksToNanoseconds)
{
    // 1 QPC tick at 10 MHz = 100 ns
    EXPECT_EQ(scale_ticks(1, kNanoPerSec, kQpcFreq), 100U);
    // 1.5 s
    EXPECT_EQ(scale_ticks(15'000'000, kNanoPerSec, kQpcFreq), 1'500'000'000U);
}

TEST(ScaleTicks, TruncatesTowardZeroLikeIntegerDivision)
{
    // 7 * 1 / 3 = 2.33.. -> 2
    EXPECT_EQ(scale_ticks(7, 1, 3), 2U);
    // 2 * 1e6 / 3 = 666666.66.. -> 666666
    EXPECT_EQ(scale_ticks(2, kMicroPerSec, 3), 666'666U);
}

// Regression: the former `ticks * 1000000000LL / freq` overflowed after ~30 min of uptime.
TEST(ScaleTicks, DoesNotOverflowForLongUptime)
{
    // 100 days of uptime at 10 MHz
    constexpr std::uint64_t seconds = 100ULL * 24ULL * 3600ULL;
    constexpr std::uint64_t ticks = seconds * kQpcFreq; // 8.64e13 ticks

    // Naive computation would overflow: ticks * 1e9 = 8.64e22 > 2^64 (~1.8e19)
    ASSERT_GT(ticks, std::numeric_limits<std::uint64_t>::max() / kNanoPerSec);

    EXPECT_EQ(scale_ticks(ticks, kNanoPerSec, kQpcFreq), seconds * kNanoPerSec);
    EXPECT_EQ(scale_ticks(ticks + 3, kNanoPerSec, kQpcFreq), seconds * kNanoPerSec + 300U);
}

TEST(ScaleTicks, MatchesStdChronoDurationCast)
{
    using qpc_ticks = std::chrono::duration<std::int64_t, std::ratio<1, kQpcFreq>>;
    for (const std::uint64_t t : {0ULL, 1ULL, 9'999'999ULL, 10'000'001ULL, 123'456'789'012ULL})
    {
        const auto expected = std::chrono::duration_cast<std::chrono::nanoseconds>(
            qpc_ticks{static_cast<std::int64_t>(t)}).count();
        EXPECT_EQ(scale_ticks(t, kNanoPerSec, kQpcFreq), static_cast<std::uint64_t>(expected)) << "t=" << t;
    }
}

#ifdef _WIN32
// Issue #226: context-switch events carry raw QPC ticks while blocks use std::chrono
// when BUILD_WITH_CHRONO_*_CLOCK is ON. Rescaling QPC ticks must land in the
// steady_clock time base, otherwise the two time bases cannot be mixed.
TEST(ScaleTicks, QpcRescaleMatchesSteadyClockTimeBase)
{
    LARGE_INTEGER freq{};
    ASSERT_NE(QueryPerformanceFrequency(&freq), FALSE);

    using period = std::chrono::steady_clock::period;

    LARGE_INTEGER qpc{};
    const auto before = std::chrono::steady_clock::now().time_since_epoch().count();
    ASSERT_NE(QueryPerformanceCounter(&qpc), FALSE);
    const auto after = std::chrono::steady_clock::now().time_since_epoch().count();

    const auto converted = scale_ticks(static_cast<std::uint64_t>(qpc.QuadPart),
                                       static_cast<std::uint64_t>(period::den),
                                       static_cast<std::uint64_t>(freq.QuadPart) * period::num);

    EXPECT_GE(converted, static_cast<std::uint64_t>(before));
    EXPECT_LE(converted, static_cast<std::uint64_t>(after));
}
#endif
