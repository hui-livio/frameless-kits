#include "HostAdapter_p.h"
#include "LinuxSystemMenu_p.h"
#include "PlatformBackend_p.h"

#include <QGuiApplication>
#include <QInputEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>

namespace FramelessKit::Internal
{
namespace
{
class LinuxBackend final : public PlatformBackend
{
public:
    using PlatformBackend::PlatformBackend;

protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        // The Wayland request must use the seat/serial belonging to this input
        // dispatch. Do not retain the event or queue the protocol request.
        const QPointer<LinuxBackend> guard(this);
        const QInputEvent* previous            = m_menuInput;
        const WaylandMenuInput previousWayland = m_waylandInput;
        if (event->type() == QEvent::MouseButtonPress)
            m_menuInput = static_cast<QMouseEvent*>(event);
        else if (event->type() == QEvent::KeyPress)
            m_menuInput = static_cast<QKeyEvent*>(event);
        else
            m_menuInput = nullptr;
        m_waylandInput = {};
#if FRAMELESSKIT_HAS_WAYLAND
        if (m_menuInput && m_menuInput->spontaneous() &&
            QGuiApplication::platformName().startsWith(QStringLiteral("wayland")))
            m_waylandInput = captureWaylandMenuInput(m_host.window());
#endif

        if (event->type() == QEvent::KeyPress &&
            (object == m_host.host() || object == m_host.window() || m_host.owns(object)))
        {
            const auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Space && key->modifiers() == Qt::AltModifier && !key->isAutoRepeat() &&
                !inputBlocked() && !m_host.fullScreen())
            {
                showSystemMenu({ 0, 0 });
                if (guard)
                {
                    m_menuInput    = previous;
                    m_waylandInput = previousWayland;
                }
                event->accept();
                return true;
            }
        }
        const bool handled = PlatformBackend::eventFilter(object, event);
        if (guard)
        {
            m_menuInput    = previous;
            m_waylandInput = previousWayland;
        }
        return handled;
    }

    void showSystemMenu(QPointF position) override
    {
#if !FRAMELESSKIT_HAS_X11 && !FRAMELESSKIT_HAS_WAYLAND
        Q_UNUSED(position);
#endif
        QWindow* window = m_host.window();
        if (!window || !window->handle() || !window->isVisible() || inputBlocked() || m_host.fullScreen())
            return;
        const QString platform = QGuiApplication::platformName();
        QString error;
        bool submitted = false;
        if (platform == QStringLiteral("xcb"))
        {
#if FRAMELESSKIT_HAS_X11
            const bool keyboard = m_menuInput && m_menuInput->type() == QEvent::KeyPress;
            submitted = showX11SystemMenu(window, position, m_menuInput ? quint32(m_menuInput->timestamp()) : 0,
                                          keyboard, error);
#else
            error = QStringLiteral("X11 system menu support was not built; enable FRAMELESSKIT_ENABLE_X11 "
                                   "with Qt's X11 native interface and XCB headers.");
#endif
        }
        else if (platform.startsWith(QStringLiteral("wayland")))
        {
#if FRAMELESSKIT_HAS_WAYLAND
            submitted = showWaylandSystemMenu(window, position, m_menuInput && m_menuInput->spontaneous(),
                                              m_waylandInput, error);
#else
            error = QStringLiteral("Wayland system menu support was not built; enable FRAMELESSKIT_ENABLE_WAYLAND "
                                   "with matching Qt GuiPrivate and Wayland client headers.");
#endif
        }
        if (!submitted && !error.isEmpty())
            m_diagnostic(error);
    }

private:
    const QInputEvent* m_menuInput = nullptr;
    WaylandMenuInput m_waylandInput;
};
} // namespace

std::unique_ptr<PlatformBackend> createPlatformBackend(HostAdapter& host, const HitTestModel& hitTest,
                                                       PlatformBackend::Diagnostic diagnostic)
{
    const QString platform = QGuiApplication::platformName();
    if (platform != QStringLiteral("xcb") && !platform.startsWith(QStringLiteral("wayland")))
        return createQtFallbackBackend(host, hitTest, std::move(diagnostic));
    auto backend = std::make_unique<LinuxBackend>(host, hitTest, std::move(diagnostic));
    backend->start();
    return backend;
}
} // namespace FramelessKit::Internal
