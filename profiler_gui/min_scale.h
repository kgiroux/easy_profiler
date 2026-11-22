/************************************************************************
* file name         : min_scale.h
* ----------------- :
* description       : Pure computation of the minimal zoom scale of the diagram
*                   : from the longest thread duration (no Qt dependency, unit-tested).
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2019  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
************************************************************************/

#ifndef EASY_PROFILER_GUI_MIN_SCALE_H
#define EASY_PROFILER_GUI_MIN_SCALE_H

#include <cmath>
#include <cstdint>

namespace profiler_gui {

/** Minimal diagram scale so that the zoom-out stops at ~110% of the longest thread.

Every 10 zoom steps is x10 in time: scalePower = 10 * log10(maxDuration * 1.1 / 1e9) + 30,
minScale = scalingCoefficientInv ^ scalePower.

\param _maxThreadDurationNs  longest thread duration in nanoseconds.
\param _scalingCoefficientInv  1 / zoom step coefficient (< 1).
\param _defaultMinScale  value used when the duration gives no information (0 ns).
\param _maxScale  upper bound of the scale (maximum zoom-in).

\return a finite, strictly positive scale not greater than _maxScale.
        Previously a 0 ns duration produced log(0) = -inf -> scale = +inf, and very short
        durations (< ~20 ns) produced a minimal scale above the maximal one.
        Long captures may still go below _defaultMinScale (original behaviour preserved).
*/
inline double computeMinScale(std::uint64_t _maxThreadDurationNs, double _scalingCoefficientInv,
                              double _defaultMinScale, double _maxScale)
{
    if (_maxThreadDurationNs == 0)
        return _defaultMinScale;

    const double scalePower = 10.0 * std::log10(static_cast<double>(_maxThreadDurationNs) * 1.10 / 1e9) + 30.0;
    const double scale = std::pow(_scalingCoefficientInv, scalePower);

    if (!std::isfinite(scale) || scale <= 0.0)
        return _defaultMinScale;
    return scale > _maxScale ? _maxScale : scale;
}

} // END of namespace profiler_gui.

#endif // EASY_PROFILER_GUI_MIN_SCALE_H
