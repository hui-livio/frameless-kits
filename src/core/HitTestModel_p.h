#pragma once
#include "FrameGeometry_p.h"
#include "HostAdapter_p.h"

#include <QPointer>
#include <QVariant>

namespace WindowHeler::Internal
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
    explicit HitTestModel(QObject* owner = nullptr) : m_owner(owner), m_hasOwner(owner != nullptr) {}

    QPointer<QObject> titleBar;
    QPointer<QObject> maximizeButton;
    QList<QPointer<QObject>> interactiveItems;
    quint64 revision = 0;

    void invalidateOwner()
    {
        m_hasOwner = true;
        m_owner.clear();
        ++revision;
    }

    HitRegion hitTest(const HostAdapter& host, QPointF point) const
    {
        const QPointer<QObject> owner = m_owner;
        const bool hasOwner           = m_hasOwner;
        const quint64 initialRevision = revision;
        // Short-circuit on the local guard before touching this model after callbacks.
        const auto current            = [this, &owner, hasOwner, initialRevision]
        { return (!hasOwner || owner) && revision == initialRevision; };
        if (!current())
            return HitRegion::Client;

        const auto titleBarSnapshot    = titleBar;
        const auto maximizeSnapshot    = maximizeButton;
        const auto interactiveSnapshot = interactiveItems;
        const bool fullscreen          = host.fullScreen();
        if (!current() || fullscreen)
            return HitRegion::Client;

        const auto contains = [&](const QPointer<QObject>& item, bool& hit)
        {
            hit = false;
            if (!current())
                return false;
            if (!item)
                return true;
            const bool owned = host.owns(item);
            if (!current())
                return false;
            if (!owned || !item)
                return true;
            hit = host.contains(item, point);
            // A custom containment mask can destroy the controller or change registrations.
            return current();
        };

        bool maximizeHit = false;
        if (!contains(maximizeSnapshot, maximizeHit))
            return HitRegion::Client;
        if (maximizeHit)
        {
            const bool maximizable = canMaximize(host);
            if (!current())
                return HitRegion::Client;
            if (maximizable && maximizeSnapshot)
            {
                const bool enabled = maximizeSnapshot->property("enabled").toBool();
                if (!current())
                    return HitRegion::Client;
                if (enabled && maximizeSnapshot)
                    return HitRegion::MaximizeButton;
            }
            // Disabled visible controls still reserve their client input area.
            return HitRegion::Client;
        }
        for (const auto& item : interactiveSnapshot)
        {
            // A registered maximize button has already been tested, even if it missed.
            if (item == maximizeSnapshot)
                continue;
            bool hit = false;
            if (!contains(item, hit) || hit)
                return HitRegion::Client;
        }
        if (titleBarSnapshot == maximizeSnapshot || interactiveSnapshot.contains(titleBarSnapshot))
            return HitRegion::Client;
        bool titleBarHit = false;
        if (!contains(titleBarSnapshot, titleBarHit))
            return HitRegion::Client;
        return titleBarHit ? HitRegion::Caption : HitRegion::Client;
    }

private:
    QPointer<QObject> m_owner;
    bool m_hasOwner = false;
};
} // namespace WindowHeler::Internal
