#pragma once
#include <QWindow>
#include <qt_windows.h>
#include <windowsx.h>
inline HWND nativeHandle(QWindow* window)
{
    return reinterpret_cast<HWND>(window->winId());
}
inline LPARAM nativePoint(HWND hwnd, QPointF point, qreal scale, bool screen = true)
{
    POINT native{ qRound(point.x() * scale), qRound(point.y() * scale) };
    if (screen)
        ClientToScreen(hwnd, &native);
    return MAKELPARAM(short(native.x), short(native.y));
}
inline LRESULT nativeHit(QWindow* window, QPointF point)
{
    return SendMessageW(nativeHandle(window), WM_NCHITTEST, 0,
                        nativePoint(nativeHandle(window), point, window->devicePixelRatio()));
}
inline void nativePress(QWindow* window, QPointF point)
{
    SendMessageW(nativeHandle(window), WM_NCMOUSEMOVE, HTMAXBUTTON,
                 nativePoint(nativeHandle(window), point, window->devicePixelRatio()));
    SendMessageW(nativeHandle(window), WM_NCLBUTTONDOWN, HTMAXBUTTON,
                 nativePoint(nativeHandle(window), point, window->devicePixelRatio()));
}
inline void nativeRelease(QWindow* window, QPointF point)
{
    SendMessageW(nativeHandle(window), WM_LBUTTONUP, 0,
                 nativePoint(nativeHandle(window), point, window->devicePixelRatio(), false));
}
