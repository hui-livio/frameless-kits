#include "WindowsMaterial_p.h"

#include <QOperatingSystemVersion>
#include <QSettings>
#include <dwmapi.h>

namespace FramelessKit::Internal
{
namespace
{
// Numeric SDK values keep builds working with pre-Windows-11 SDK headers.
constexpr DWORD systemBackdropAttribute = 38;
constexpr DWORD immersiveDarkAttribute  = 20;
int windowsBuild()
{
    return QOperatingSystemVersion::current().microVersion();
}
QString failure(const char* operation, HRESULT result)
{
    return QStringLiteral("%1 failed (HRESULT 0x%2); using Solid.")
        .arg(QString::fromLatin1(operation), QString::number(quint32(result), 16));
}
#if FRAMELESSKIT_ENABLE_LEGACY_ACCENT
struct AccentPolicy
{
    int state;
    int flags;
    DWORD gradient;
    int animation;
};
struct CompositionData
{
    int attribute;
    void* data;
    SIZE_T size;
};
bool accent(HWND window, int state, const QColor& color)
{
    using SetComposition       = BOOL(WINAPI*)(HWND, CompositionData*);
    static auto setComposition = reinterpret_cast<SetComposition>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (!setComposition)
        return false;
    const DWORD tint =
        (DWORD(180) << 24) | (DWORD(color.blue()) << 16) | (DWORD(color.green()) << 8) | DWORD(color.red());
    AccentPolicy policy{ state, state == 4 ? 2 : 0, state == 4 ? tint : 0, 0 };
    CompositionData data{ 19, &policy, sizeof(policy) };
    return setComposition(window, &data);
}
#endif
} // namespace
void WindowsMaterial::bind(HWND window)
{
    m_window       = window;
    m_haveBackdrop = SUCCEEDED(
        DwmGetWindowAttribute(window, systemBackdropAttribute, &m_originalBackdrop, sizeof(m_originalBackdrop)));
    m_haveDark =
        SUCCEEDED(DwmGetWindowAttribute(window, immersiveDarkAttribute, &m_originalDark, sizeof(m_originalDark)));
    m_valid = false;
}
bool WindowsMaterial::clear(QString& error)
{
    bool ok = true;
#if FRAMELESSKIT_ENABLE_LEGACY_ACCENT
    if (m_accentChanged)
    {
        if (accent(m_window, 0, Qt::white))
            m_accentChanged = false;
        else
        {
            ok    = false;
            error = QStringLiteral("Cannot clear the previous Accent material.");
        }
    }
#endif
    if (m_backdropChanged)
    {
        const int none = 1; // DWMSBT_NONE, not AUTO which may choose another material.
        HRESULT hr     = DwmSetWindowAttribute(m_window, systemBackdropAttribute, &none, sizeof(none));
        if (SUCCEEDED(hr))
        {
            m_backdropChanged = false;
            m_lastBackdrop    = none;
        }
        else
        {
            ok    = false;
            error = failure("DwmSetWindowAttribute(NONE)", hr);
        }
    }
    if (m_blurChanged)
    {
        DWM_BLURBEHIND blur{};
        blur.dwFlags = DWM_BB_ENABLE;
        HRESULT hr   = DwmEnableBlurBehindWindow(m_window, &blur);
        if (SUCCEEDED(hr))
            m_blurChanged = false;
        else
        {
            ok    = false;
            error = failure("DwmEnableBlurBehindWindow(disable)", hr);
        }
    }
    m_applied = Material::Solid;
    m_valid   = false;
    return ok;
}
bool WindowsMaterial::restoreDark(QString& error)
{
    if (!m_darkChanged || !m_haveDark)
        return true;
    BOOL currentDark = FALSE;
    if (FAILED(DwmGetWindowAttribute(m_window, immersiveDarkAttribute, &currentDark, sizeof(currentDark))))
    {
        error = QStringLiteral("Cannot query the owned DWM dark appearance for restoration.");
        return false;
    }
    // Leave a later application-owned appearance change untouched.
    if (currentDark == m_lastDark)
    {
        const HRESULT hr =
            DwmSetWindowAttribute(m_window, immersiveDarkAttribute, &m_originalDark, sizeof(m_originalDark));
        if (FAILED(hr))
        {
            error = failure("DwmSetWindowAttribute(restore dark appearance)", hr);
            return false;
        }
    }
    m_darkChanged = false;
    return true;
}
void WindowsMaterial::release(bool restore)
{
    if (restore && m_window && IsWindow(m_window))
    {
        QString error;
        int currentBackdrop           = 0;
        const bool backdropStillOwned = m_backdropOwned &&
                                        SUCCEEDED(DwmGetWindowAttribute(m_window, systemBackdropAttribute,
                                                                        &currentBackdrop, sizeof(currentBackdrop))) &&
                                        currentBackdrop == m_lastBackdrop;
        const bool restoreBackdrop = backdropStillOwned && m_haveBackdrop;
        // clear() must not erase a backdrop subsequently set by the application.
        if (!backdropStillOwned)
            m_backdropChanged = false;
        clear(error);
        if (restoreBackdrop)
            DwmSetWindowAttribute(m_window, systemBackdropAttribute, &m_originalBackdrop, sizeof(m_originalBackdrop));
        restoreDark(error);
    }
    m_window          = nullptr;
    m_backdropChanged = m_blurChanged = m_accentChanged = m_darkChanged = false;
    m_haveBackdrop = m_haveDark = m_backdropOwned = false;
    m_valid                                       = false;
}
bool WindowsMaterial::apply(Material material, const QColor& color, QString& error)
{
    if (!m_window)
        return material == Material::Solid;
    if (material != Material::Solid)
    {
        BOOL composition = FALSE;
        HIGHCONTRASTW contrast{ sizeof(HIGHCONTRASTW), 0, nullptr };
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        const QSettings settings(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
            QSettings::NativeFormat);
        if (FAILED(DwmIsCompositionEnabled(&composition)) || !composition || GetSystemMetrics(SM_REMOTESESSION) ||
            (contrast.dwFlags & HCF_HIGHCONTRASTON) ||
            !settings.value(QStringLiteral("EnableTransparency"), 1).toBool())
        {
            error = QStringLiteral("DWM material disabled by composition, remote session, high contrast or system "
                                   "transparency policy; using Solid.");
            return false;
        }
    }
    if (m_valid && m_applied == material && m_color == color)
        return true;
    if (!clear(error))
        return false;
    if (material == Material::Solid)
    {
        if (!restoreDark(error))
            return false;
    }
    else
    {
        const BOOL dark = color.lightnessF() < 0.5;
        if (m_haveDark && SUCCEEDED(DwmSetWindowAttribute(m_window, immersiveDarkAttribute, &dark, sizeof(dark))))
        {
            m_darkChanged = true;
            m_lastDark    = dark;
        }
        if (material == Material::Mica || material == Material::MicaAlt ||
            (material == Material::Acrylic && windowsBuild() >= 22621))
        {
            if (windowsBuild() < 22621)
            {
                error = QStringLiteral("Mica/MicaAlt requires Windows 11 22H2 (build 22621) or later; using Solid.");
                return false;
            }
            const int type = material == Material::Mica ? 2 : material == Material::Acrylic ? 3 : 4;
            HRESULT hr     = DwmSetWindowAttribute(m_window, systemBackdropAttribute, &type, sizeof(type));
            if (FAILED(hr))
            {
                error = failure("DwmSetWindowAttribute(backdrop)", hr);
                return false;
            }
            m_backdropChanged = m_backdropOwned = true;
            m_lastBackdrop                      = type;
        }
        else if (material == Material::Blur || material == Material::Acrylic)
        {
#if FRAMELESSKIT_ENABLE_LEGACY_ACCENT
            if (windowsBuild() < 10240 || (material == Material::Acrylic && windowsBuild() < 17134) ||
                !accent(m_window, material == Material::Blur ? 3 : 4, color))
            {
                error = QStringLiteral("Legacy Accent material is unavailable or rejected by Windows; using Solid.");
                return false;
            }
            m_accentChanged = true;
#else
            error =
                QStringLiteral("Blur / pre-22H2 Acrylic requires FRAMELESSKIT_ENABLE_LEGACY_ACCENT=ON; using Solid.");
            return false;
#endif
        }
        // An empty blur region enables alpha composition without adding a second
        // blur effect. Qt's native (non-layered) Widgets surface keeps its DWM frame.
        DWM_BLURBEHIND blur{};
        blur.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        blur.fEnable  = TRUE;
        blur.hRgnBlur = CreateRectRgn(0, 0, 0, 0);
        if (!blur.hRgnBlur)
        {
            error = QStringLiteral("Cannot allocate the DWM alpha region.");
            return false;
        }
        HRESULT hr = DwmEnableBlurBehindWindow(m_window, &blur);
        DeleteObject(blur.hRgnBlur);
        if (FAILED(hr))
        {
            error = failure("DwmEnableBlurBehindWindow(alpha)", hr);
            return false;
        }
        m_blurChanged = true;
    }
    m_applied = material;
    m_color   = color;
    m_valid   = true;
    return true;
}
} // namespace FramelessKit::Internal
