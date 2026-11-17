#pragma once
#include "FrameGeometry_p.h"
#include "HostAdapter_p.h"

#include <QPointer>
#include <QVariant>
#include <algorithm>

namespace FramelessKit::Internal
{
enum class HitRegion
{
    Client,
    Caption,
    MaximizeButton
};
// Logical client coordinates only. Native edge/DPI handling belongs to the
// backend.
class HitTestModel
{
public:
    QPointer<QObject> titleBar;
    QPointer<QObject> maximizeButton;
    QList<QPointer<QObject>> interactiveItems;

    HitRegion hitTest(const HostAdapter& host, QPointF point) const
    {
        if (host.fullScreen())
            return HitRegion::Client;
        auto contains = [&](const QPointer<QObject>& item)
        { return item && host.owns(item) && host.contains(item, point); };
        if (contains(maximizeButton) && canMaximize(host) && maximizeButton->property("enabled").toBool())
            return HitRegion::MaximizeButton;
        // Disabled visible controls still reserve their client input area.
        if (contains(maximizeButton) || std::ranges::any_of(interactiveItems, contains))
            return HitRegion::Client;
        return contains(titleBar) ? HitRegion::Caption : HitRegion::Client;
    }
};
} // namespace FramelessKit::Internal
