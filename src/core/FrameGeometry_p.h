#pragma once
#include "HostAdapter_p.h"

namespace FramelessKit::Internal
{
inline bool windowCapability(const HostAdapter& host, Qt::WindowType hint)
{
    return host.windowCapability(hint);
}
inline bool canResizeWidth(const HostAdapter& host)
{
    return !host.flags().testFlag(Qt::MSWindowsFixedSizeDialogHint) &&
           host.minimumSize().width() < host.maximumSize().width();
}
inline bool canResizeHeight(const HostAdapter& host)
{
    return !host.flags().testFlag(Qt::MSWindowsFixedSizeDialogHint) &&
           host.minimumSize().height() < host.maximumSize().height();
}
inline bool canMaximize(const HostAdapter& host)
{
    return windowCapability(host, Qt::WindowMaximizeButtonHint) && canResizeWidth(host) && canResizeHeight(host);
}
// Both the point and the extent use the same units (physical pixels on Windows).
inline Qt::Edges resizeEdges(QPointF point, QSizeF extent, QSizeF border, bool horizontal, bool vertical,
                             bool maximized, bool fullscreen)
{
    if (maximized || fullscreen || point.x() < 0 || point.y() < 0 || point.x() >= extent.width() ||
        point.y() >= extent.height())
        return {};
    Qt::Edges edges;
    if (horizontal)
    {
        if (point.x() < border.width())
            edges |= Qt::LeftEdge;
        else if (point.x() >= extent.width() - border.width())
            edges |= Qt::RightEdge;
    }
    if (vertical)
    {
        if (point.y() < border.height())
            edges |= Qt::TopEdge;
        else if (point.y() >= extent.height() - border.height())
            edges |= Qt::BottomEdge;
    }
    return edges;
}
// Subtract the physical client origin BEFORE applying the scale. Desktop origins
// remain in native coordinates, including monitors to the left/above the primary.
inline QPointF logicalClientPoint(QPointF nativeScreen, QPointF nativeClientOrigin, qreal scale)
{
    return (nativeScreen - nativeClientOrigin) / scale;
}
} // namespace FramelessKit::Internal
