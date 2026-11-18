#pragma once
#include <FramelessKit/Material.h>
#include <QColor>
#include <QString>
#include <qt_windows.h>

namespace FramelessKit::Internal
{
// All undocumented Accent declarations are confined to WindowsMaterial.cpp.
class WindowsMaterial
{
public:
    void bind(HWND window);
    void release(bool restore);
    bool apply(Material material, const QColor& color, QString& error);
    Material applied() const
    {
        return m_applied;
    }
    static bool usesSystemBackdrop(Material material);
    void invalidate()
    {
        m_valid = false;
    }

private:
    bool clear(QString& error);
    bool restoreDark(QString& error);
    HWND m_window      = nullptr;
    Material m_applied = Material::Solid;
    QColor m_color;
    bool m_valid           = false;
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
} // namespace FramelessKit::Internal
