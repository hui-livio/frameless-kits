#pragma once
#include <WindowHeler/Material.h>
#include <QColor>
#include <QString>
#include <qt_windows.h>

namespace WindowHeler::Internal
{
// All undocumented Accent declarations are confined to WindowsMaterial.cpp.
class WindowsMaterial
{
public:
    void bind(HWND window);
    void release(bool restore);
    bool apply(Material material, const QColor& color, QString& error);
    static bool usesSystemBackdrop(Material material);

private:
    bool clear(QString& error);
    bool restoreDark(QString& error);
    HWND m_window      = nullptr;
    bool m_accentChanged   = false;
    bool m_blurChanged     = false;
    bool m_backdropChanged = false;
    bool m_backdropOwned   = false;
    int m_lastBackdrop     = 1;
    bool m_darkChanged     = false;
    bool m_haveBackdrop    = false;
    bool m_haveDark        = false;
    int m_originalBackdrop = 1;
    BOOL m_originalDark    = FALSE;
    BOOL m_lastDark        = FALSE;
};
} // namespace WindowHeler::Internal
