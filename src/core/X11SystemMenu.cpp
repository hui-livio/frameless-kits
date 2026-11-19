#include "LinuxSystemMenu_p.h"

#include <QGuiApplication>
#include <QLibrary>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>
#include <xcb/xcb.h>

namespace WindowHeler::Internal
{
namespace
{
struct XcbApi
{
    QLibrary library{ QStringLiteral("libxcb.so.1") };
    QString loadError;
    bool available                                                       = false;
    decltype(&xcb_connection_has_error) connectionHasError               = nullptr;
    decltype(&xcb_get_geometry) getGeometry                              = nullptr;
    decltype(&xcb_get_geometry_reply) getGeometryReply                   = nullptr;
    decltype(&xcb_intern_atom) internAtom                                = nullptr;
    decltype(&xcb_intern_atom_reply) internAtomReply                     = nullptr;
    decltype(&xcb_get_property) getProperty                              = nullptr;
    decltype(&xcb_get_property_reply) getPropertyReply                   = nullptr;
    decltype(&xcb_get_property_value) getPropertyValue                   = nullptr;
    decltype(&xcb_get_property_value_length) getPropertyValueLength      = nullptr;
    decltype(&xcb_translate_coordinates) translateCoordinates            = nullptr;
    decltype(&xcb_translate_coordinates_reply) translateCoordinatesReply = nullptr;
    decltype(&xcb_ungrab_pointer_checked) ungrabPointerChecked           = nullptr;
    decltype(&xcb_send_event_checked) sendEventChecked                   = nullptr;
    decltype(&xcb_request_check) requestCheck                            = nullptr;
    decltype(&xcb_flush) flush                                           = nullptr;

    XcbApi()
    {
        library.setLoadHints(QLibrary::PreventUnloadHint);
        if (!library.load())
        {
            loadError =
                QStringLiteral("Cannot load libxcb.so.1 for the X11 window menu: %1.").arg(library.errorString());
            return;
        }
        available = resolve(connectionHasError, "xcb_connection_has_error") &&
                    resolve(getGeometry, "xcb_get_geometry") && resolve(getGeometryReply, "xcb_get_geometry_reply") &&
                    resolve(internAtom, "xcb_intern_atom") && resolve(internAtomReply, "xcb_intern_atom_reply") &&
                    resolve(getProperty, "xcb_get_property") && resolve(getPropertyReply, "xcb_get_property_reply") &&
                    resolve(getPropertyValue, "xcb_get_property_value") &&
                    resolve(getPropertyValueLength, "xcb_get_property_value_length") &&
                    resolve(translateCoordinates, "xcb_translate_coordinates") &&
                    resolve(translateCoordinatesReply, "xcb_translate_coordinates_reply") &&
                    resolve(ungrabPointerChecked, "xcb_ungrab_pointer_checked") &&
                    resolve(sendEventChecked, "xcb_send_event_checked") && resolve(requestCheck, "xcb_request_check") &&
                    resolve(flush, "xcb_flush");
    }

    template <typename Function> bool resolve(Function& function, const char* name)
    {
        function = reinterpret_cast<Function>(library.resolve(name));
        if (function)
            return true;
        loadError = QStringLiteral("Cannot resolve %1 in libxcb.so.1: %2.")
                        .arg(QString::fromLatin1(name), library.errorString());
        return false;
    }
};

struct XcbFree
{
    void operator()(void* pointer) const
    {
        std::free(pointer);
    }
};
template <typename T> using XcbPointer = std::unique_ptr<T, XcbFree>;

bool checkResult(const XcbApi& api, xcb_connection_t* connection, const xcb_generic_error_t* nativeError,
                 const char* operation, QString& error)
{
    if (nativeError)
    {
        error = QStringLiteral("X11 %1 failed (protocol error %2).")
                    .arg(QString::fromLatin1(operation))
                    .arg(int(nativeError->error_code));
        return false;
    }
    const int connectionError = api.connectionHasError(connection);
    if (connectionError)
    {
        error = QStringLiteral("X11 %1 failed (connection error %2).")
                    .arg(QString::fromLatin1(operation))
                    .arg(connectionError);
        return false;
    }
    return true;
}

template <typename Function, typename Cookie>
auto readReply(const XcbApi& api, xcb_connection_t* connection, Function function, Cookie cookie, const char* operation,
               QString& error)
{
    xcb_generic_error_t* rawError = nullptr;
    auto* rawReply                = function(connection, cookie, &rawError);
    const XcbPointer<xcb_generic_error_t> nativeError(rawError);
    XcbPointer<std::remove_pointer_t<decltype(rawReply)>> reply(rawReply);
    if (!checkResult(api, connection, nativeError.get(), operation, error))
        reply.reset();
    else if (!reply)
        error = QStringLiteral("X11 %1 returned no reply.").arg(QString::fromLatin1(operation));
    return reply;
}

bool checkRequest(const XcbApi& api, xcb_connection_t* connection, xcb_void_cookie_t cookie, const char* operation,
                  QString& error)
{
    const XcbPointer<xcb_generic_error_t> nativeError(api.requestCheck(connection, cookie));
    return checkResult(api, connection, nativeError.get(), operation, error);
}

xcb_atom_t existingAtom(const XcbApi& api, xcb_connection_t* connection, const char* name, QString& error)
{
    // Atom existence alone is not capability detection; also inspect _NET_SUPPORTED.
    const auto reply = readReply(api, connection, api.internAtomReply,
                                 api.internAtom(connection, 1, quint16(std::strlen(name)), name), name, error);
    if (!reply)
        return XCB_ATOM_NONE;
    if (reply->atom == XCB_ATOM_NONE)
        error = QStringLiteral("The X11 window menu is unavailable: atom %1 does not exist.")
                    .arg(QString::fromLatin1(name));
    return reply->atom;
}
} // namespace

bool showX11SystemMenu(QWindow* window, QPointF localPosition, quint32 timestamp, bool keyboard, QString& error)
{
    error.clear();
    if (!window || !window->handle() || !window->isVisible())
    {
        error = QStringLiteral("X11 window menu requires an existing, visible native window.");
        return false;
    }
    auto* native     = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    auto* connection = native ? native->connection() : nullptr;
    if (!connection)
    {
        error = QStringLiteral("The active Qt platform does not expose an X11 connection.");
        return false;
    }
    static const XcbApi api;
    if (!api.available)
    {
        error = api.loadError;
        return false;
    }
    if (!checkResult(api, connection, nullptr, "window menu", error))
        return false;

    // winId() cannot create a platform window after the handle check above.
    const WId id = window->winId();
    if (!id || id > std::numeric_limits<xcb_window_t>::max())
    {
        error = QStringLiteral("The native window does not have a valid X11 window ID.");
        return false;
    }
    const auto xid = static_cast<xcb_window_t>(id);
    const auto geometry =
        readReply(api, connection, api.getGeometryReply, api.getGeometry(connection, xid), "window geometry", error);
    if (!geometry)
        return false;
    const xcb_window_t root = geometry->root;
    if (root == XCB_WINDOW_NONE)
    {
        error = QStringLiteral("The X11 window has no root window.");
        return false;
    }

    const xcb_atom_t menuAtom = existingAtom(api, connection, "_GTK_SHOW_WINDOW_MENU", error);
    if (menuAtom == XCB_ATOM_NONE)
        return false;
    const xcb_atom_t supportedAtom = existingAtom(api, connection, "_NET_SUPPORTED", error);
    if (supportedAtom == XCB_ATOM_NONE)
        return false;
    const auto supported = readReply(api, connection, api.getPropertyReply,
                                     api.getProperty(connection, 0, root, supportedAtom, XCB_ATOM_ATOM, 0, 16384),
                                     "_NET_SUPPORTED", error);
    if (!supported)
        return false;
    if (supported->type != XCB_ATOM_ATOM || supported->format != 32 || supported->bytes_after != 0)
    {
        error = QStringLiteral("The X11 root window has no complete ATOM/32 _NET_SUPPORTED property.");
        return false;
    }
    const int valueBytes = api.getPropertyValueLength(supported.get());
    const auto* atoms    = static_cast<const xcb_atom_t*>(api.getPropertyValue(supported.get()));
    if (valueBytes < 0 || quint64(valueBytes) != quint64(supported->value_len) * sizeof(xcb_atom_t) ||
        (supported->value_len && !atoms))
    {
        error = QStringLiteral("The X11 root window has a malformed _NET_SUPPORTED property.");
        return false;
    }
    if (!supported->value_len ||
        std::find(atoms, atoms + supported->value_len, menuAtom) == atoms + supported->value_len)
    {
        error = QStringLiteral("The window manager does not advertise _GTK_SHOW_WINDOW_MENU on this window's root.");
        return false;
    }

    const auto origin =
        readReply(api, connection, api.translateCoordinatesReply, api.translateCoordinates(connection, xid, root, 0, 0),
                  "window root coordinates", error);
    if (!origin)
        return false;
    if (!origin->same_screen)
    {
        error = QStringLiteral("The X11 window and its root are not on the same screen.");
        return false;
    }
    const qreal dpr = window->devicePixelRatio();
    if (!std::isfinite(dpr) || dpr <= 0 || !std::isfinite(localPosition.x()) || !std::isfinite(localPosition.y()))
    {
        error = QStringLiteral("The X11 window menu position or device pixel ratio is invalid.");
        return false;
    }
    // Scale only the local offset. The translated origin is already in native
    // pixels and may be negative; scaling Qt global coordinates would distort it.
    const qreal rootX      = std::round(qreal(origin->dst_x) + localPosition.x() * dpr);
    const qreal rootY      = std::round(qreal(origin->dst_y) + localPosition.y() * dpr);
    constexpr auto minimum = std::numeric_limits<qint32>::min();
    constexpr auto maximum = std::numeric_limits<qint32>::max();
    if (!std::isfinite(rootX) || !std::isfinite(rootY) || rootX < minimum || rootX > maximum || rootY < minimum ||
        rootY > maximum)
    {
        error = QStringLiteral("The X11 window menu root coordinates are outside the signed 32-bit range.");
        return false;
    }

    xcb_client_message_event_t event{};
    event.response_type  = XCB_CLIENT_MESSAGE;
    event.format         = 32;
    event.window         = xid;
    event.type           = menuAtom;
    event.data.data32[0] = keyboard ? 0u : 3u;
    // Preserve signed coordinates in the 32-bit wire fields.
    event.data.data32[1] = static_cast<quint32>(static_cast<qint32>(rootX));
    event.data.data32[2] = static_cast<quint32>(static_cast<qint32>(rootY));

    // Release a mouse grab only after every capability and position check passed.
    if (!keyboard &&
        !checkRequest(api, connection, api.ungrabPointerChecked(connection, timestamp), "pointer ungrab", error))
        return false;
    const auto sent = api.sendEventChecked(connection, 0, root,
                                           XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                                           reinterpret_cast<const char*>(&event));
    if (!checkRequest(api, connection, sent, "window menu request", error))
        return false;
    const int flushed = api.flush(connection);
    if (!checkResult(api, connection, nullptr, "window menu flush", error))
        return false;
    if (flushed <= 0)
    {
        error = QStringLiteral("The X11 window menu request could not be flushed.");
        return false;
    }
    // The server accepted the request; this does not acknowledge a visible menu.
    return true;
}
} // namespace WindowHeler::Internal
