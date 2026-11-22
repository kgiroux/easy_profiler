/**
Lightweight profiler library for c++
Copyright(C) 2016-2019  Sergey Yagovtsev, Victor Zarubkin

Licensed under either of
    * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
    * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
at your option.
**/

#ifndef EASY_PROFILER_TICK_CONVERSION_H
#define EASY_PROFILER_TICK_CONVERSION_H

#include <cstdint>
#include <easy/details/easy_compiler_support.h>

namespace profiler { namespace detail {

/** Computes `ticks * multiplier / divisor` without intermediate 64-bit overflow.

The naive expression `ticks * multiplier / divisor` overflows as soon as
`ticks * multiplier > 2^64` (e.g. QueryPerformanceCounter at 10 MHz converted
to nanoseconds overflows after ~30 minutes of system uptime).

The value is split into a whole part and a remainder:
    ticks = q * divisor + r   (0 <= r < divisor)
    ticks * multiplier / divisor = q * multiplier + r * multiplier / divisor

This is the same decomposition used by MSVC's std::chrono::steady_clock,
so converting raw QPC ticks with it yields exactly the steady_clock value.

\pre (divisor - 1) * multiplier fits into uint64_t.
\return 0 if divisor == 0.

\note Kept C++11-compatible (single return statement) because the core library is built as C++11.
*/
EASY_CONSTEXPR_FCN inline uint64_t scale_ticks(uint64_t ticks, uint64_t multiplier, uint64_t divisor) EASY_NOEXCEPT
{
    return divisor == 0 ? 0 : (ticks / divisor) * multiplier + ((ticks % divisor) * multiplier) / divisor;
}

} } // end of namespace profiler::detail.

#endif // EASY_PROFILER_TICK_CONVERSION_H
