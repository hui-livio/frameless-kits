#pragma once
#include <WindowHeler/Material.h>
#include <QCursor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <array>
#include <functional>
#include <memory>

class QWindow;

namespace WindowHeler::Internal
{
class HostAdapter;
class HitTestModel;

class PlatformBackend : public QObject
{
public:
    using Diagnostic = std::function<void(const QString&)>;
    PlatformBackend(HostAdapter& host, const HitTestModel& hitTest, Diagnostic diagnostic);
    ~PlatformBackend() override;
    void start(bool frameless);
    void setFramelessEnabled(bool enabled);
    virtual bool applyMaterial(Material material, QString& error);
    virtual bool materialReady() const;
    void setMaterialRefresh(std::function<void()> refresh)
    {
        m_materialRefresh = std::move(refresh);
    }
    virtual void resetInput();
    bool inputBlocked() const;

protected:
    bool eventFilter(QObject* object, QEvent* event) override;
    virtual bool framelessHint() const
    {
        return true;
    }
    virtual bool clientResizeEnabled() const
    {
        return true;
    }
    virtual void showSystemMenu(QPointF position);
    virtual void titleBarDoubleClick();
    virtual void paintNativeFrame() {}
    virtual void synchronizeNative() {}
    virtual void releaseNative(bool) {}
    virtual bool nativeHandling() const
    {
        return false;
    }
    void scheduleSynchronize();
    HostAdapter& m_host;
    const HitTestModel& m_hitTest;
    Diagnostic m_diagnostic;
    quint64 m_generation = 0;
    bool m_framelessEnabled = false;

private:
    void observeCursorWindow(QWindow* window);
    void restoreCursor();
    bool updateCursor(QWindow* window, Qt::Edges edges);
    std::function<void()> m_materialRefresh;
    bool m_changedFrameless  = false;
    bool m_originalFrameless = false;
    bool m_pending           = false;
    bool m_surfaceDestroying = false;
    bool m_cursorChanged     = false;
    QCursor m_originalCursor;
    QPointer<QWindow> m_cursorWindow;
    QPointer<QWindow> m_observedCursorWindow;
    std::array<QMetaObject::Connection, 5> m_cursorConnections;
    enum class MouseState
    {
        Idle,
        PreparingMove
    };
    MouseState m_mouseState = MouseState::Idle;
    bool m_forwardingPaint  = false;
};
std::unique_ptr<PlatformBackend> createQtFallbackBackend(HostAdapter&, const HitTestModel&,
                                                         PlatformBackend::Diagnostic);
std::unique_ptr<PlatformBackend> createPlatformBackend(HostAdapter&, const HitTestModel&, PlatformBackend::Diagnostic);
} // namespace WindowHeler::Internal
