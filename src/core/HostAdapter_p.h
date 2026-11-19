#pragma once
#include <QColor>
#include <QEvent>
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QWindow>

namespace WindowHeler::Internal
{
// Implemented by UI adapters; Core never includes Widgets or Quick headers.
class HostWindowAdapter
{
public:
    virtual ~HostWindowAdapter() = default;
    virtual QObject* host() const                                         = 0;
    // Queries must never create a native surface.
    virtual QWindow* window() const                                       = 0;
    virtual bool fullScreen() const                                       = 0;
    virtual Qt::WindowFlags flags() const
    {
        return window() ? window()->flags() : Qt::WindowFlags{};
    }
    virtual bool windowCapability(Qt::WindowType hint) const
    {
        // QWindow defaults can omit individual caption hints. Widgets exposes
        // normalized flags and overrides this to preserve explicit opt-outs.
        return !flags().testFlag(Qt::CustomizeWindowHint) || flags().testFlag(hint);
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
};

class HostInputAdapter
{
public:
    virtual ~HostInputAdapter()                                           = default;
    virtual bool owns(const QObject* item) const                          = 0;
    virtual bool contains(const QObject* item, QPointF windowPoint) const = 0;
    virtual bool supportsMaximizeButton(const QObject*) const
    {
        return false;
    }
    // Mouse positions are logical client coordinates. No business command is executed here.
    virtual void buttonEvent(QObject*, QEvent::Type, QPointF, Qt::MouseButton, Qt::MouseButtons) {}
    virtual void cancelButton(QObject*) {}
};

class HostMaterialAdapter
{
public:
    virtual ~HostMaterialAdapter() = default;
    // Reserve alpha before first native surface creation; never create a handle.
    virtual void prepareMaterialSurface() {}
    virtual void materialSurfaceDestroyed() {}
    virtual bool materialSurfaceAvailable(QWindow* window, QString& error) const
    {
        if (window && window->format().alphaBufferSize() > 0)
            return true;
        error = QStringLiteral("Material requires an alpha surface. Bind the Agent before first show; "
                               "an existing opaque surface is not recreated implicitly.");
        return false;
    }
    virtual QColor materialColor() const
    {
        return QColor(Qt::white);
    }
    virtual void setMaterialBackground(bool) {}
    virtual void setNativeTopInset(bool) {}
};

class HostAdapter : public HostWindowAdapter, public HostInputAdapter, public HostMaterialAdapter
{
public:
    ~HostAdapter() override = default;
};
} // namespace WindowHeler::Internal
