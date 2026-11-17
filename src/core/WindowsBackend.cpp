#include "FrameGeometry_p.h"
#include "HitTestModel_p.h"
#include "PlatformBackend_p.h"
#include "WindowsMaterial_p.h"

#include <QGuiApplication>
#include <QOperatingSystemVersion>
#include <QPointer>
#include <QTimer>
#include <QWindow>
// Windows declarations must precede commctrl; alphabetical include sorting breaks the SDK headers.
// clang-format off
#include <qt_windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>
// clang-format on

namespace FramelessKit::Internal
{
namespace
{
constexpr UINT_PTR subclassId     = 0x46524d4b;
constexpr LONG_PTR frameStyleMask = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
int edgeHit(Qt::Edges edges)
{
    if (edges == (Qt::TopEdge | Qt::LeftEdge))
        return HTTOPLEFT;
    if (edges == (Qt::TopEdge | Qt::RightEdge))
        return HTTOPRIGHT;
    if (edges == (Qt::BottomEdge | Qt::LeftEdge))
        return HTBOTTOMLEFT;
    if (edges == (Qt::BottomEdge | Qt::RightEdge))
        return HTBOTTOMRIGHT;
    if (edges == Qt::LeftEdge)
        return HTLEFT;
    if (edges == Qt::RightEdge)
        return HTRIGHT;
    if (edges == Qt::TopEdge)
        return HTTOP;
    if (edges == Qt::BottomEdge)
        return HTBOTTOM;
    return HTCLIENT;
}
class WindowsBackend final : public PlatformBackend
{
public:
    using PlatformBackend::PlatformBackend;
    ~WindowsBackend() override
    {
        releaseNative(true);
    }
    bool applyMaterial(Material material, QString& error) override
    {
        const bool ok = m_material.apply(material, m_host.materialColor(), error);
        if (m_hwnd)
            updateDwmFrame();
        return ok;
    }
    bool materialReady() const override
    {
        return m_hwnd != nullptr;
    }
    void resetInput() override
    {
        const bool captured      = m_pressed;
        m_pressed                = false;
        QPointer<QObject> button = m_button;
        m_button.clear();
        if (captured && m_hwnd && GetCapture() == m_hwnd)
            ReleaseCapture();
        if (button)
            m_host.cancelButton(button);
    }

protected:
    bool framelessHint() const override { return false; }
    bool nativeHandling() const override
    {
        return m_hwnd != nullptr;
    }
    void synchronizeNative() override
    {
        auto* window = m_host.window();
        // handle() is a non-creating public query. winId() is only safe after it succeeds.
        HWND hwnd    = window && window->handle() ? reinterpret_cast<HWND>(window->winId()) : nullptr;
        bool newBinding = false;
        if (m_hwnd != hwnd)
        {
            releaseNative(true);
            if (!hwnd)
                return;
            m_hwnd = hwnd;
            ++m_generation;
            newBinding = true;
            m_haveOriginalPolicy = SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                                                    &m_originalPolicy, sizeof(m_originalPolicy)));
            m_originalStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
            m_appliedStyle  = m_originalStyle;
            if (!SetWindowSubclass(hwnd, subclassProc, subclassId, reinterpret_cast<DWORD_PTR>(this)))
            {
                m_hwnd = nullptr;
                m_diagnostic(QStringLiteral("Cannot install the native window subclass; using Qt frame handling."));
                return;
            }
        }
        if (!m_hwnd)
            return;
        if (newBinding)
            m_material.bind(m_hwnd);
        const auto previous = GetWindowLongPtrW(m_hwnd, GWL_STYLE);
        LONG_PTR desired    = WS_CAPTION | WS_SYSMENU;
        if (!m_host.fullScreen())
        {
            // Fixed-size windows still need a thick frame for the native DWM border/shadow.
            // Resizing is controlled by hit testing and WM_GETMINMAXINFO, not by removing the frame.
            desired |= WS_THICKFRAME;
            if (canMaximize(m_host))
                desired |= WS_MAXIMIZEBOX;
            if (windowCapability(m_host, Qt::WindowMinimizeButtonHint))
                desired |= WS_MINIMIZEBOX;
        }
        m_appliedStyle   = (previous & ~frameStyleMask) | desired;
        const auto state = m_host.states();
        if (newBinding || previous != m_appliedStyle || state != m_lastState)
        {
            RECT clientBefore{}; GetClientRect(m_hwnd, &clientBefore);
            POINT originBefore{}; ClientToScreen(m_hwnd, &originBefore);
            const bool preserveClient = !state && !IsIconic(m_hwnd) && !IsZoomed(m_hwnd);
            m_lastState = state;
            SetWindowLongPtrW(m_hwnd, GWL_STYLE, m_appliedStyle);
            SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            if (preserveClient) preserveClientGeometry(clientBefore, originBefore);
        }
        updateDwmFrame();
    }
    void releaseNative(bool restore) override
    {
        resetInput();
        m_material.release(restore);
        HWND hwnd = m_hwnd;
        m_hwnd    = nullptr;
        ++m_generation;
        if (!hwnd || !IsWindow(hwnd))
            return;
        RECT clientBefore{}; GetClientRect(hwnd, &clientBefore);
        POINT originBefore{}; ClientToScreen(hwnd, &originBefore);
        RemoveWindowSubclass(hwnd, subclassProc, subclassId);
        if (restore)
        {
            DWMNCRENDERINGPOLICY currentPolicy{};
            if (m_haveOriginalPolicy && SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                    &currentPolicy, sizeof(currentPolicy))) && currentPolicy == DWMNCRP_ENABLED)
                DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &m_originalPolicy, sizeof(m_originalPolicy));
            MARGINS margins{}; DwmExtendFrameIntoClientArea(hwnd, &margins);
            const auto current = GetWindowLongPtrW(hwnd, GWL_STYLE);
            // Restore only bits we actually changed and which still match our last write.
            const auto owned   = (m_originalStyle ^ m_appliedStyle) & frameStyleMask & ~(current ^ m_appliedStyle);
            SetWindowLongPtrW(hwnd, GWL_STYLE, (current & ~owned) | (m_originalStyle & owned));
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            if (!m_host.states() && !IsIconic(hwnd) && !IsZoomed(hwnd))
                preserveClientGeometry(clientBefore, originBefore, hwnd);
        }
    }

private:
    void preserveClientGeometry(const RECT& clientBefore, POINT originBefore, HWND target = nullptr)
    {
        HWND hwnd = target ? target : m_hwnd;
        RECT clientAfter{}, frame{};
        POINT originAfter{};
        GetClientRect(hwnd, &clientAfter); GetWindowRect(hwnd, &frame); ClientToScreen(hwnd, &originAfter);
        const int dx = originBefore.x - originAfter.x, dy = originBefore.y - originAfter.y;
        const int dw = clientBefore.right - clientAfter.right, dh = clientBefore.bottom - clientAfter.bottom;
        if (dx || dy || dw || dh)
            SetWindowPos(hwnd, nullptr, frame.left + dx, frame.top + dy,
                         frame.right - frame.left + dw, frame.bottom - frame.top + dh,
                         SWP_NOZORDER | SWP_NOACTIVATE);
    }
    void updateDwmFrame()
    {
        BOOL composition = FALSE;
        if (FAILED(DwmIsCompositionEnabled(&composition)) || !composition) return;
        const DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
        DwmSetWindowAttribute(m_hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
        // The side/bottom frame is real non-client area. Windows 10 additionally
        // needs the removed top edge extended into the client for native rendering.
        MARGINS margins{};
        if (!m_host.fullScreen() && !IsZoomed(m_hwnd) && !isWindows11())
            margins.cyTopHeight = 1;
        // Extend from the left only: negative/full top margins can resurrect the
        // hidden caption when Windows accent-colored title bars are enabled.
        if (m_material.extendsFrame())
            margins.cxLeftWidth = 65536;
        DwmExtendFrameIntoClientArea(m_hwnd, &margins);
    }
    static bool isWindows11()
    {
        static const bool value = QOperatingSystemVersion::current().microVersion() >= 22000;
        return value;
    }
    void drawNativeTopBorder()
    {
        if (!m_hwnd || isWindows11() || m_host.fullScreen() || IsZoomed(m_hwnd) || IsIconic(m_hwnd)) return;
        HDC dc = GetDC(m_hwnd);
        if (!dc) return;
        RECT rect{}; GetClientRect(m_hwnd, &rect); rect.bottom = 1;
        FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        ReleaseDC(m_hwnd, dc);
    }
    static LRESULT CALLBACK subclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR,
                                         DWORD_PTR reference)
    {
        auto* self = reinterpret_cast<WindowsBackend*>(reference);
        if (message == WM_NCDESTROY)
        {
            self->releaseNative(false);
            return DefSubclassProc(hwnd, message, wParam, lParam);
        }
        LRESULT result = 0;
        // A forwarded Qt button event can destroy the Agent. Never access self afterwards.
        if (self->process(message, wParam, lParam, result))
            return result;
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }
    UINT dpi() const
    {
        using GetDpi = UINT(WINAPI*)(HWND);
        static const auto getDpi =
            reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandleW(L"user32"), "GetDpiForWindow"));
        return getDpi ? getDpi(m_hwnd) : 96;
    }
    QSize border() const
    {
        using GetMetrics = int(WINAPI*)(int, UINT);
        static const auto metrics =
            reinterpret_cast<GetMetrics>(GetProcAddress(GetModuleHandleW(L"user32"), "GetSystemMetricsForDpi"));
        const auto metric = [&](int value) { return metrics ? metrics(value, dpi()) : GetSystemMetrics(value); };
        return { metric(SM_CXSIZEFRAME) + metric(SM_CXPADDEDBORDER),
                 metric(SM_CYSIZEFRAME) + metric(SM_CXPADDEDBORDER) };
    }
    QPointF logicalPoint(POINT screen) const
    {
        POINT origin{ 0, 0 };
        ClientToScreen(m_hwnd, &origin);
        return logicalClientPoint({ qreal(screen.x), qreal(screen.y) }, { qreal(origin.x), qreal(origin.y) },
                                  m_host.window()->devicePixelRatio());
    }
    POINT screenPoint(UINT message, LPARAM param) const
    {
        POINT point{ GET_X_LPARAM(param), GET_Y_LPARAM(param) };
        if (message == WM_MOUSEMOVE || message == WM_LBUTTONUP)
            ClientToScreen(m_hwnd, &point);
        return point;
    }
    int hitTest(POINT screen) const
    {
        if (inputBlocked() || m_host.fullScreen())
            return HTCLIENT;
        POINT client = screen;
        ScreenToClient(m_hwnd, &client);
        RECT outer{}; GetWindowRect(m_hwnd, &outer);
        if (!IsZoomed(m_hwnd))
        {
            const auto nativeEdges = resizeEdges({qreal(screen.x - outer.left), qreal(screen.y - outer.top)},
                {qreal(outer.right - outer.left), qreal(outer.bottom - outer.top)}, border(),
                canResizeWidth(m_host), canResizeHeight(m_host), false, false);
            if (nativeEdges) return edgeHit(nativeEdges);
        }
        RECT rect{};
        GetClientRect(m_hwnd, &rect);
        const auto edges = resizeEdges({ qreal(client.x), qreal(client.y) }, { qreal(rect.right), qreal(rect.bottom) },
                                       border(), canResizeWidth(m_host), canResizeHeight(m_host),
                                       IsZoomed(m_hwnd) || m_host.states().testFlag(Qt::WindowMaximized), false);
        if (edges)
            return edgeHit(edges);
        switch (m_hitTest.hitTest(m_host, logicalPoint(screen)))
        {
        case HitRegion::Caption:
            return HTCAPTION;
        case HitRegion::MaximizeButton:
            return HTMAXBUTTON;
        default:
            return HTCLIENT;
        }
    }
    void dispatchCommand(UINT command)
    {
        const quint64 generation = m_generation;
        QTimer::singleShot(0, this,
                           [this, command, generation]
                           {
                               if (generation != m_generation || !m_hwnd || inputBlocked())
                                   return;
                               if (command == SC_CLOSE)
                               {
                                   m_host.close();
                                   return;
                               }
                               if (command == SC_MINIMIZE && windowCapability(m_host, Qt::WindowMinimizeButtonHint))
                                   m_host.setStates(Qt::WindowMinimized);
                               else if (command == SC_MAXIMIZE && canMaximize(m_host))
                                   m_host.setStates(Qt::WindowMaximized);
                               else if (command == SC_RESTORE)
                                   m_host.setStates(Qt::WindowNoState);
                           });
    }
    void systemMenu(POINT point)
    {
        HMENU menu = GetSystemMenu(m_hwnd, FALSE);
        if (!menu)
            return;
        const bool maximized = IsZoomed(m_hwnd);
        auto enable          = [&](UINT command, bool value)
        { EnableMenuItem(menu, command, MF_BYCOMMAND | (value ? MF_ENABLED : MF_GRAYED)); };
        enable(SC_RESTORE, maximized || IsIconic(m_hwnd));
        enable(SC_MOVE, !maximized);
        enable(SC_SIZE, !maximized && (canResizeWidth(m_host) || canResizeHeight(m_host)));
        enable(SC_MINIMIZE, windowCapability(m_host, Qt::WindowMinimizeButtonHint));
        enable(SC_MAXIMIZE, !maximized && canMaximize(m_host));
        enable(SC_CLOSE, windowCapability(m_host, Qt::WindowCloseButtonHint));
        QPointer<WindowsBackend> guard(this);
        const UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, m_hwnd, nullptr);
        if (guard && command)
            PostMessageW(m_hwnd, WM_SYSCOMMAND, command, 0);
    }
    void trackHover(POINT screen)
    {
        QPointer<QObject> target = hitTest(screen) == HTMAXBUTTON ? m_hitTest.maximizeButton : nullptr;
        if (m_button != target)
        {
            QPointer<WindowsBackend> guard(this);
            resetInput();
            if (!guard)
                return;
            m_button = target;
            if (target)
                m_host.buttonEvent(target, QEvent::Enter, logicalPoint(screen), Qt::NoButton, {});
        }
        if (m_button)
        {
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE | TME_NONCLIENT, m_hwnd, 0 };
            TrackMouseEvent(&track);
        }
    }
    bool process(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
    {
        result = 0;
        switch (message)
        {
        case WM_NCCALCSIZE:
        {
            RECT requested =
                wParam ? reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0] : *reinterpret_cast<RECT*>(lParam);
            // Qt must see the message first to update its own frame/margin bookkeeping.
            DefSubclassProc(m_hwnd, message, wParam, lParam);
            RECT adjusted = requested;
            if (!m_host.fullScreen())
            {
                // Preserve the real DWM left/right/bottom frame. Remove only the caption.
                // Qt's procedure already ran above; use a separate native calculation so
                // its frameless/geometry bookkeeping cannot erase the remaining frame.
                if (wParam)
                {
                    NCCALCSIZE_PARAMS native = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                    native.rgrc[0] = requested;
                    DefWindowProcW(m_hwnd, WM_NCCALCSIZE, TRUE, reinterpret_cast<LPARAM>(&native));
                    adjusted = native.rgrc[0];
                }
                else
                    DefWindowProcW(m_hwnd, WM_NCCALCSIZE, FALSE, reinterpret_cast<LPARAM>(&adjusted));
                adjusted.top = requested.top;
                if (IsZoomed(m_hwnd)) adjusted.top += border().height();
                else if (isWindows11())
                {
                    UINT thickness = 1;
                    // Windows 11 DWMWA_VISIBLE_FRAME_BORDER_THICKNESS; retain its native edge.
                    DwmGetWindowAttribute(m_hwnd, 37, &thickness, sizeof(thickness));
                    adjusted.top += LONG(thickness);
                    adjusted.bottom += LONG(thickness);
                }
                if (IsZoomed(m_hwnd))
                {
                    MONITORINFO monitor{sizeof(monitor)};
                    HMONITOR handle = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
                    if (GetMonitorInfoW(handle, &monitor))
                    {
                        for (UINT edge = ABE_LEFT; edge <= ABE_BOTTOM; ++edge)
                        {
                            APPBARDATA data{sizeof(data)};
                            data.uEdge = edge; data.rc = monitor.rcMonitor;
                            if (!SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &data)) continue;
                            if (edge == ABE_LEFT) adjusted.left += 2;
                            if (edge == ABE_TOP) adjusted.top += 2;
                            if (edge == ABE_RIGHT) adjusted.right -= 2;
                            if (edge == ABE_BOTTOM) adjusted.bottom -= 2;
                        }
                    }
                }
            }
            if (wParam)
                reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0] = adjusted;
            else
                *reinterpret_cast<RECT*>(lParam) = adjusted;
            return true;
        }
        case WM_NCHITTEST:
            result = hitTest({ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
            return true;
        case WM_PAINT:
            DefSubclassProc(m_hwnd, message, wParam, lParam);
            drawNativeTopBorder();
            return true;
        case WM_SETTINGCHANGE:
        case WM_DWMCOMPOSITIONCHANGED:
            m_material.invalidate();
            scheduleSynchronize();
            return false;
        case WM_THEMECHANGED:
            m_material.invalidate();
            scheduleSynchronize();
            return false;
        /* frame-related changes retain normal Qt processing */
        case WM_DWMNCRENDERINGCHANGED:
        case WM_DPICHANGED:
        case WM_STYLECHANGED:
        case WM_SIZE:
            scheduleSynchronize();
            return false; // Preserve Qt DPI, min/max and geometry handling.
        case WM_CANCELMODE:
        case WM_KILLFOCUS:
            resetInput();
            return false;
        case WM_CAPTURECHANGED:
            if (reinterpret_cast<HWND>(lParam) != m_hwnd)
                resetInput();
            return false;
        case WM_NCMOUSELEAVE:
            if (!m_pressed)
                resetInput();
            return false;
        case WM_NCMOUSEMOVE:
            if (!m_pressed)
                trackHover(screenPoint(message, lParam));
            return false; // DefWindowProc still receives HTMAXBUTTON for Snap Layout.
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
            if (wParam == HTMAXBUTTON && !inputBlocked())
            {
                QPointer<WindowsBackend> guard(this);
                resetInput();
                if (!guard)
                    return true;
                const POINT point = screenPoint(message, lParam);
                if (hitTest(point) != HTMAXBUTTON)
                    return true;
                m_button  = m_hitTest.maximizeButton;
                m_pressed = true;
                SetCapture(m_hwnd);
                m_host.buttonEvent(m_button, QEvent::MouseButtonPress, logicalPoint(point), Qt::LeftButton,
                                   Qt::LeftButton);
                return true;
            }
            return false;
        case WM_MOUSEMOVE:
            if (m_pressed)
            {
                if (inputBlocked() || !m_button || !m_host.owns(m_button))
                {
                    resetInput();
                    return true;
                }
                m_host.buttonEvent(m_button, QEvent::MouseMove, logicalPoint(screenPoint(message, lParam)),
                                   Qt::NoButton, Qt::LeftButton);
                return true;
            }
            return false;
        case WM_NCLBUTTONUP:
        case WM_LBUTTONUP:
            if (m_pressed)
            {
                QPointer<QObject> button = m_button;
                const POINT point        = screenPoint(message, lParam);
                m_pressed                = false;
                m_button.clear();
                if (GetCapture() == m_hwnd)
                    ReleaseCapture();
                if (button && m_host.owns(button) && !inputBlocked())
                    m_host.buttonEvent(button, QEvent::MouseButtonRelease, logicalPoint(point), Qt::LeftButton, {});
                else if (button)
                    m_host.cancelButton(button);
                return true;
            }
            // Suppress a stray release belonging to the bridged non-client button sequence.
            return message == WM_NCLBUTTONUP && wParam == HTMAXBUTTON;
        case WM_NCRBUTTONUP:
            if (wParam == HTCAPTION && !inputBlocked())
            {
                systemMenu(screenPoint(message, lParam));
                return true;
            }
            return false;
        case WM_SYSCOMMAND:
        {
            const UINT command = UINT(wParam) & 0xfff0;
            if (command == SC_KEYMENU && lParam == VK_SPACE && !inputBlocked() && !m_host.fullScreen())
            {
                POINT point{ 0, 0 };
                ClientToScreen(m_hwnd, &point);
                systemMenu(point);
                return true;
            }
            if (command == SC_MAXIMIZE || command == SC_MINIMIZE || command == SC_RESTORE || command == SC_CLOSE)
            {
                if (command != SC_CLOSE || windowCapability(m_host, Qt::WindowCloseButtonHint))
                    dispatchCommand(command);
                return true;
            }
            return false;
        }
        default:
            return false;
        }
    }
    WindowsMaterial m_material;
    DWMNCRENDERINGPOLICY m_originalPolicy = DWMNCRP_USEWINDOWSTYLE;
    bool m_haveOriginalPolicy = false;
    HWND m_hwnd              = nullptr;
    LONG_PTR m_originalStyle = 0;
    LONG_PTR m_appliedStyle  = 0;
    Qt::WindowStates m_lastState;
    QPointer<QObject> m_button;
    bool m_pressed = false;
};
} // namespace
std::unique_ptr<PlatformBackend> createPlatformBackend(HostAdapter& host, const HitTestModel& hitTest,
                                                       PlatformBackend::Diagnostic diagnostic)
{
    if (QGuiApplication::platformName() != QStringLiteral("windows"))
        return createQtFallbackBackend(host, hitTest, std::move(diagnostic));
    auto backend = std::make_unique<WindowsBackend>(host, hitTest, std::move(diagnostic));
    backend->start();
    return backend;
}
} // namespace FramelessKit::Internal
