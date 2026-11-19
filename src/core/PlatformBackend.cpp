#include "FrameGeometry_p.h"
#include "HitTestModel_p.h"
#include "PlatformBackend_p.h"

#include <QCursor>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPlatformSurfaceEvent>
#include <QStyleHints>
#include <QTimer>
#include <QWindow>

namespace WindowHeler::Internal
{
PlatformBackend::PlatformBackend(HostAdapter& host, const HitTestModel& hitTest, Diagnostic diagnostic)
    : m_host(host), m_hitTest(hitTest), m_diagnostic(std::move(diagnostic))
{
}
PlatformBackend::~PlatformBackend()
{
    qApp->removeEventFilter(this);
    restoreCursor();
    if (m_changedFrameless && m_host.host())
        m_host.setFrameless(m_originalFrameless);
}
void PlatformBackend::start()
{
    m_originalFrameless = m_host.flags().testFlag(Qt::FramelessWindowHint);
    m_changedFrameless  = m_originalFrameless != framelessHint();
    if (m_changedFrameless)
        m_host.setFrameless(framelessHint());
    qApp->installEventFilter(this);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { scheduleSynchronize(); });
    synchronizeNative();
}
void PlatformBackend::resetInput()
{
    m_mouseState = MouseState::Idle;
    restoreCursor();
}
void PlatformBackend::observeCursorWindow(QWindow* window)
{
    if (m_observedCursorWindow == window)
        return;
    for (const auto& connection : m_cursorConnections)
        disconnect(connection);
    m_observedCursorWindow = window;
    // QWidget may update constraints without these signals; mouse presses and
    // releases also recheck them, even when there has been no hover movement.
    m_cursorConnections    = {
        connect(window, &QWindow::minimumWidthChanged, this, [this] { restoreCursor(); }),
        connect(window, &QWindow::minimumHeightChanged, this, [this] { restoreCursor(); }),
        connect(window, &QWindow::maximumWidthChanged, this, [this] { restoreCursor(); }),
        connect(window, &QWindow::maximumHeightChanged, this, [this] { restoreCursor(); }),
        connect(window, &QWindow::windowStateChanged, this, [this] { resetInput(); }),
    };
}
void PlatformBackend::restoreCursor()
{
    if (!m_cursorChanged)
        return;
    const auto window   = m_cursorWindow;
    const QCursor saved = m_originalCursor;
    // CursorChange is synchronous and may reenter or destroy the backend.
    m_cursorChanged     = false;
    m_cursorWindow.clear();
    if (window)
        window->setCursor(saved);
}
bool PlatformBackend::updateCursor(QWindow* window, Qt::Edges edges)
{
    const QPointer<PlatformBackend> guard(this);
    const QPointer<QWindow> target(window);
    if (!edges || (m_cursorChanged && m_cursorWindow != window))
    {
        restoreCursor();
        if (!guard || !target || m_host.window() != target)
            return false;
        if (!edges)
            return true;
    }
    observeCursorWindow(window);
    const bool horizontal = edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge);
    const bool vertical   = edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge);
    Qt::CursorShape shape = horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor;
    if (horizontal && vertical)
        shape = edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)
                    ? Qt::SizeFDiagCursor
                    : Qt::SizeBDiagCursor;
    if (!m_cursorChanged)
    {
        m_originalCursor = window->cursor();
        m_cursorWindow   = window;
        m_cursorChanged  = true;
    }
    window->setCursor(shape);
    return guard && target && m_host.window() == target;
}
void PlatformBackend::showSystemMenu(QPointF)
{
    m_diagnostic(QStringLiteral("The current platform does not provide a native window menu."));
}
void PlatformBackend::titleBarDoubleClick()
{
    if (canMaximize(m_host))
        m_host.setStates(m_host.states() ^ Qt::WindowMaximized);
}
bool PlatformBackend::applyMaterial(Material material, QString& error)
{
    if (material == Material::Solid || material == Material::Transparent)
        return true;
    error = QStringLiteral("The %1 platform has no native backend for the requested material; using Solid.")
                .arg(QGuiApplication::platformName());
    return false;
}
bool PlatformBackend::materialReady() const
{
    return m_host.window() && m_host.window()->handle();
}
bool PlatformBackend::inputBlocked() const
{
    QWindow* window = m_host.window();
    if (!window || !window->isVisible())
        return true;
    if (QWindow* modal = QGuiApplication::modalWindow(); modal && modal != window)
    {
        if (modal->modality() == Qt::ApplicationModal)
            return true;
        for (auto* parent = modal->transientParent(); parent; parent = parent->transientParent())
            if (parent == window)
                return true;
    }
    for (auto* top : QGuiApplication::topLevelWindows())
        if (top != window && top->isVisible() && top->type() == Qt::Popup)
            return true;
    return false;
}
void PlatformBackend::scheduleSynchronize()
{
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this,
                       [this]
                       {
                           m_pending = false;
                           const QPointer<PlatformBackend> guard(this);
                           if (!m_surfaceDestroying)
                               synchronizeNative();
                           if (!guard)
                               return;
                           const auto refresh = m_materialRefresh;
                           if (refresh)
                               refresh();
                       });
}
bool PlatformBackend::eventFilter(QObject* object, QEvent* event)
{
    if (m_forwardingPaint)
        return false;
    const QPointer<PlatformBackend> guard(this);
    const QPointer<QObject> receiver(object);
    if (object == qApp && (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::ThemeChange))
        scheduleSynchronize();
    const bool isWindow = object == m_host.window();
    const bool isHost   = object == m_host.host();
    if (isWindow && event->type() == QEvent::PlatformSurface)
    {
        const auto type     = static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType();
        m_surfaceDestroying = type == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed;
        if (m_surfaceDestroying)
        {
            ++m_generation;
            m_host.materialSurfaceDestroyed();
            resetInput();
            if (!guard || !receiver)
                return true;
            releaseNative(false);
        }
        else
            synchronizeNative();
        if (!guard || !receiver)
            return true;
        scheduleSynchronize();
    }
    if (nativeHandling() &&
        ((isHost && event->type() == QEvent::UpdateRequest) || (isWindow && event->type() == QEvent::Expose)))
    {
        // Let Qt complete its raster update before exposing the native top edge.
        m_forwardingPaint = true;
        QCoreApplication::sendEvent(object, event);
        if (!guard)
            return true;
        m_forwardingPaint = false;
        if (!receiver)
            return true;
        paintNativeFrame();
        return true;
    }
    if (isWindow || isHost)
    {
        switch (event->type())
        {
        case QEvent::WinIdChange:
            resetInput();
            if (!guard || !receiver)
                return true;
            [[fallthrough]];
        case QEvent::Show:
            m_surfaceDestroying = false;
            scheduleSynchronize();
            break;
        case QEvent::PaletteChange:
        case QEvent::ApplicationPaletteChange:
        case QEvent::ThemeChange:
        case QEvent::Resize:
        case QEvent::ScreenChangeInternal:
            scheduleSynchronize();
            break;
        case QEvent::WindowStateChange:
            resetInput();
            if (!guard || !receiver)
                return true;
            scheduleSynchronize();
            break;
        case QEvent::Hide:
        case QEvent::WindowDeactivate:
        case QEvent::UngrabMouse:
            resetInput();
            if (!guard || !receiver)
                return true;
            break;
        default:
            break;
        }
    }
    if (nativeHandling() || (!isWindow && !isHost && !m_host.owns(object)))
        return false;
    const QPointer<QWindow> window = m_host.window();
    if (!window || inputBlocked() || m_host.fullScreen())
    {
        resetInput();
        return !guard || !receiver;
    }
    if (event->type() == QEvent::Leave && m_cursorChanged)
    {
        restoreCursor();
        if (!guard || !receiver || !window)
            return true;
    }
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick &&
        event->type() != QEvent::MouseMove && event->type() != QEvent::MouseButtonRelease)
        return false;
    auto* mouse             = static_cast<QMouseEvent*>(event);
    const QPointF point     = window->mapFromGlobal(mouse->globalPosition());
    const auto currentEdges = [&]
    {
        return resizeEdges(point, window->size(), { 6, 6 }, clientResizeEnabled() && canResizeWidth(m_host),
                           clientResizeEnabled() && canResizeHeight(m_host),
                           m_host.states().testFlag(Qt::WindowMaximized), m_host.fullScreen());
    };
    auto edges = currentEdges();
    if (event->type() == QEvent::MouseMove && !mouse->buttons())
    {
        return !updateCursor(window, edges) || !receiver;
    }
    if (event->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton)
    {
        const bool handled = m_mouseState != MouseState::Idle;
        m_mouseState       = MouseState::Idle;
        if (!updateCursor(window, edges) || !receiver)
            return true;
        if (handled)
            event->accept();
        return handled;
    }
    if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton)
    {
        if (!updateCursor(window, edges) || !receiver)
            return true;
        // A CursorChange handler may have changed constraints or window state.
        if (inputBlocked() || m_host.fullScreen())
        {
            resetInput();
            return !guard || !receiver;
        }
        edges = currentEdges();
    }
    if (event->type() == QEvent::MouseMove && mouse->buttons().testFlag(Qt::LeftButton))
    {
        if (m_mouseState == MouseState::PreparingMove)
        {
            m_mouseState       = MouseState::SystemOperation;
            const bool started = window->startSystemMove();
            if (!guard || !receiver || !window)
                return true;
            if (!started)
            {
                m_mouseState = MouseState::Idle;
                m_diagnostic(QStringLiteral("System move is not supported by this platform."));
            }
            event->accept();
            return true;
        }
        return m_mouseState == MouseState::SystemOperation;
    }
    if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton && edges)
    {
        m_mouseState       = MouseState::SystemOperation;
        const bool started = window->startSystemResize(edges);
        if (!guard || !receiver || !window)
            return true;
        if (!started)
        {
            m_mouseState = MouseState::Idle;
            m_diagnostic(QStringLiteral("System resize is not supported by this platform."));
        }
        event->accept();
        return true;
    }
    const auto region = m_hitTest.hitTest(m_host, point);
    // Quick containment masks may execute application code during hit testing.
    if (!guard || !receiver || !window)
        return true;
    if (region != HitRegion::Caption)
        return false;
    if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::RightButton)
    {
        // Wayland needs the serial from this input event; do not queue the request.
        showSystemMenu(point);
        event->accept();
        return true;
    }
    if (mouse->button() != Qt::LeftButton)
        return false;
    if (event->type() == QEvent::MouseButtonDblClick)
    {
        m_mouseState = MouseState::Idle;
        titleBarDoubleClick();
        event->accept();
        return true;
    }
    if (event->type() == QEvent::MouseButtonPress)
    {
        // A stationary click must still deliver a release and allow a double click.
        m_mouseState = MouseState::PreparingMove;
        event->accept();
        return true;
    }
    return false;
}
} // namespace WindowHeler::Internal
