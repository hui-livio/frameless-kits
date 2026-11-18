#include "LinuxSystemMenu_p.h"

#include <QGuiApplication>
#include <QLibrary>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>
// Qt 6.8+ has no public QWindow API exposing xdg_toplevel or the Qt-only
// coordinate scale. Keep these two QPA details in this optional adapter.
#include <QtGui/private/qhighdpiscaling_p.h>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <limits>
#include <wayland-client-core.h>

namespace FramelessKit::Internal
{
namespace
{
struct WaylandApi
{
    QLibrary library{ QStringLiteral("wayland-client"), 0 };
    decltype(&wl_proxy_marshal_flags) marshal    = nullptr;
    decltype(&wl_proxy_get_version) version      = nullptr;
    decltype(&wl_proxy_get_class) proxyClass     = nullptr;
    decltype(&wl_display_get_error) displayError = nullptr;
    decltype(&wl_display_flush) flush            = nullptr;

    WaylandApi()
    {
        library.setLoadHints(QLibrary::PreventUnloadHint);
        if (!library.load())
            return;
        marshal      = reinterpret_cast<decltype(marshal)>(library.resolve("wl_proxy_marshal_flags"));
        version      = reinterpret_cast<decltype(version)>(library.resolve("wl_proxy_get_version"));
        proxyClass   = reinterpret_cast<decltype(proxyClass)>(library.resolve("wl_proxy_get_class"));
        displayError = reinterpret_cast<decltype(displayError)>(library.resolve("wl_display_get_error"));
        flush        = reinterpret_cast<decltype(flush)>(library.resolve("wl_display_flush"));
    }
    bool available() const
    {
        return marshal && version && proxyClass && displayError && flush;
    }
};
} // namespace

WaylandMenuInput captureWaylandMenuInput(QWindow* window)
{
    if (!window || !window->handle())
        return {};
    auto* native   = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    auto* platform = QGuiApplication::platformNativeInterface();
    if (!native || !platform)
        return {};
    return { native->display(), native->lastInputSeat(),
             static_cast<wl_proxy*>(platform->nativeResourceForWindow("xdg_toplevel", window)),
             native->lastInputSerial() };
}

bool showWaylandSystemMenu(QWindow* window, QPointF localPosition, bool hasInputEvent, const WaylandMenuInput& input,
                           QString& error)
{
    error.clear();
    if (!window || !window->handle() || !window->isVisible())
    {
        error = QStringLiteral("Wayland window menu requires an existing, visible native surface.");
        return false;
    }
    auto* native   = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    auto* platform = QGuiApplication::platformNativeInterface();
    if (!native || !platform || !native->display())
    {
        error = QStringLiteral("The active Qt platform does not expose the Wayland native interface.");
        return false;
    }
    const uint serial = native->lastInputSerial();
    wl_seat* seat     = native->lastInputSeat();
    if (!hasInputEvent || !serial || !seat)
    {
        error = QStringLiteral("Wayland window menu requires a real input event with a valid seat and serial.");
        return false;
    }
    auto* toplevel = static_cast<wl_proxy*>(platform->nativeResourceForWindow("xdg_toplevel", window));
    if (!toplevel)
    {
        error = QStringLiteral("The Wayland shell does not expose an xdg_toplevel window menu.");
        return false;
    }
    // Hit testing can call application code (e.g. a Quick containment mask).
    // If it processes nested input or recreates the surface, do not apply that
    // newer event's credentials to the outer menu request.
    if (input.display != native->display() || input.seat != seat || input.serial != serial ||
        input.toplevel != toplevel)
    {
        error = QStringLiteral("Wayland input or native surface changed during window menu hit testing.");
        return false;
    }
    static const WaylandApi api;
    if (!api.available())
    {
        error = QStringLiteral("Required libwayland-client window menu functions are unavailable.");
        return false;
    }
    const char* name = api.proxyClass(toplevel);
    if (!name || std::strcmp(name, "xdg_toplevel") != 0 || api.version(toplevel) < 1 ||
        api.displayError(native->display()))
    {
        error = QStringLiteral("Wayland window menu is unavailable for this shell or failed connection.");
        return false;
    }
    // xdg-shell uses surface-local coordinates. Undo only Qt's scaling;
    // multiplying by devicePixelRatio would also apply the compositor's buffer scale.
    const QPointF surfacePosition = QHighDpi::toNativeLocalPosition(localPosition, window);
    constexpr auto limit          = std::numeric_limits<qint32>::max();
    if (!std::isfinite(surfacePosition.x()) || !std::isfinite(surfacePosition.y()) ||
        std::abs(surfacePosition.x()) >= limit || std::abs(surfacePosition.y()) >= limit)
    {
        error = QStringLiteral("Wayland window menu position is outside the protocol coordinate range.");
        return false;
    }
    // xdg_toplevel.show_window_menu is request 4, available since version 1.
    // This request creates no object; the marshaler's null return is not failure.
    api.marshal(toplevel, 4, nullptr, api.version(toplevel), 0, seat, serial, qint32(qRound(surfacePosition.x())),
                qint32(qRound(surfacePosition.y())));
    if (api.displayError(native->display()))
    {
        error = QStringLiteral("The Wayland connection failed while queuing the window menu request.");
        return false;
    }
    if (api.flush(native->display()) < 0 && errno != EAGAIN)
    {
        error = QStringLiteral("Cannot flush the Wayland window menu request.");
        return false;
    }
    // On EAGAIN, Qt owns the display and its next flush will send the queued data.
    return true;
}
} // namespace FramelessKit::Internal
