/************************************************************************
* file name         : qt_compat.h
* ----------------- :
* description       : Small compatibility layer allowing profiler_gui to be built
*                   : with both Qt 5 (>= 5.12) and Qt 6.
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2019  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
************************************************************************/

#ifndef EASY_PROFILER_GUI_QT_COMPAT_H
#define EASY_PROFILER_GUI_QT_COMPAT_H

#include <QtGlobal>
#include <QEvent>
#include <QLabel>
#include <QPixmap>
#include <QPointF>
#include <QWheelEvent>

#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
# error "profiler_gui requires Qt 5.12 or newer (or Qt 6)"
#endif

namespace profiler_gui {

/// Argument type of QWidget::enterEvent(): QEvent* in Qt 5, QEnterEvent* in Qt 6.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using EnterEvent = QEnterEvent;
#else
using EnterEvent = QEvent;
#endif

/// QLabel::pixmap() returns `const QPixmap*` in Qt 5 and `QPixmap` (by value) in Qt 6.
inline QPixmap labelPixmap(const QLabel& _label)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return _label.pixmap();
#elif QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return _label.pixmap(Qt::ReturnByValue);
#else
    const QPixmap* pixmap = _label.pixmap(); // may be null when no pixmap is set
    return pixmap != nullptr ? *pixmap : QPixmap();
#endif
}

/// Cursor position of a wheel event: QWheelEvent::position() exists since Qt 5.14 (posF() before).
inline QPointF wheelPosition(const QWheelEvent& _event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return _event.position();
#else
    return _event.posF();
#endif
}

} // END of namespace profiler_gui.

#endif // EASY_PROFILER_GUI_QT_COMPAT_H
