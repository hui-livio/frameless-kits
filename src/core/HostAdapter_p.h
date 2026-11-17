#pragma once
#include <QObject>
#include <QPointF>
class QWindow;

namespace FramelessKit::Internal
{
// Implemented by UI adapters; Core never includes Widgets or Quick headers.
class HostAdapter
{
public:
    virtual ~HostAdapter()                                                = default;
    virtual QObject* host() const                                         = 0;
    // Must not create a native surface or call winId().
    virtual QWindow* window() const                                       = 0;
    virtual bool owns(const QObject* item) const                          = 0;
    virtual bool contains(const QObject* item, QPointF windowPoint) const = 0;
    virtual bool fullScreen() const                                       = 0;
};
} // namespace FramelessKit::Internal
