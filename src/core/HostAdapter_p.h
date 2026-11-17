#pragma once
#include <QEvent>
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QWindow>

namespace FramelessKit::Internal
{
// Implemented by UI adapters; Core never includes Widgets or Quick headers.
class HostAdapter
{
public:
    virtual ~HostAdapter()                                                = default;
    virtual QObject* host() const                                         = 0;
    // Queries must never create a native surface.
    virtual QWindow* window() const                                       = 0;
    virtual bool owns(const QObject* item) const                          = 0;
    virtual bool contains(const QObject* item, QPointF windowPoint) const = 0;
    virtual bool fullScreen() const                                       = 0;
    virtual Qt::WindowFlags flags() const
    {
        return window() ? window()->flags() : Qt::WindowFlags{};
    }
    virtual Qt::WindowStates states() const
    {
        return window() ? window()->windowStates() : Qt::WindowStates{};
    }
    virtual QSize minimumSize() const
    {
        return window() ? window()->minimumSize() : QSize{};
    }
    virtual QSize maximumSize() const
    {
        return window() ? window()->maximumSize() : QSize(16777215, 16777215);
    }
    virtual void setFrameless(bool enabled)
    {
        if (window())
            window()->setFlag(Qt::FramelessWindowHint, enabled);
    }
    virtual void setStates(Qt::WindowStates state)
    {
        if (window())
            window()->setWindowStates(state);
    }
    virtual void close()
    {
        if (window())
            window()->close();
    }
    virtual bool supportsMaximizeButton(const QObject*) const
    {
        return false;
    }
    // Mouse positions are logical client coordinates. No business command is executed here.
    virtual void buttonEvent(QObject*, QEvent::Type, QPointF, Qt::MouseButton, Qt::MouseButtons) {}
    virtual void cancelButton(QObject*) {}
};
} // namespace FramelessKit::Internal
