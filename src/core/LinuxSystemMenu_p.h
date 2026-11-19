#pragma once

#include <QPointF>
#include <QString>
#include <QtGlobal>

class QWindow;
struct wl_display;
struct wl_seat;
struct wl_proxy;

namespace WindowHeler::Internal
{
// A true result means the request was submitted, not that the compositor made
// the menu visible. On failure, error describes the detected problem.
bool showX11SystemMenu(QWindow* window, QPointF localPosition, quint32 timestamp, bool keyboard, QString& error);
struct WaylandMenuInput
{
    wl_display* display = nullptr;
    wl_seat* seat       = nullptr;
    wl_proxy* toplevel  = nullptr;
    quint32 serial      = 0;
};
WaylandMenuInput captureWaylandMenuInput(QWindow* window);
bool showWaylandSystemMenu(QWindow* window, QPointF localPosition, bool hasInputEvent, const WaylandMenuInput& input,
                           QString& error);
} // namespace WindowHeler::Internal
