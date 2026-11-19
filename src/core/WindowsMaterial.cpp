#include "WindowsMaterial_p.h"

#include <QOperatingSystemVersion>
#include <QSettings>
#include <dwmapi.h>

namespace WindowHeler::Internal
{
namespace
{
// Numeric SDK values keep builds working with pre-Windows-11 SDK headers.
constexpr DWORD systemBackdropAttribute = 38;
int windowsBuild()
{
    return QOperatingSystemVersion::current().microVersion();
}
DWORD immersiveDarkAttribute()
{
    return windowsBuild() >= 19041 ? 20 : 19;
}
HRESULT setDarkAppearance(HWND window, BOOL dark)
{
    if (windowsBuild() < 17763)
        return E_NOTIMPL;
    // qwindowkit's dark-mode path also opts the process into native theme
    // support and flushes menu themes. DWMWA_USE_IMMERSIVE_DARK_MODE alone
    // can leave the first native popup/frame using cached light colors.
    static const HMODULE theme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (theme)
    {
        if (windowsBuild() >= 18362)
        {
            using SetPreferredAppMode = int(WINAPI*)(int);
            static const auto setPreferredAppMode = reinterpret_cast<SetPreferredAppMode>(
                GetProcAddress(theme, MAKEINTRESOURCEA(135)));
            if (setPreferredAppMode)
                setPreferredAppMode(1); // PAM_AUTO: support the system theme, never force all Qt controls dark.
        }
        else
        {
            using AllowDarkModeForApp = BOOL(WINAPI*)(BOOL);
            static const auto allowDarkModeForApp = reinterpret_cast<AllowDarkModeForApp>(
                GetProcAddress(theme, MAKEINTRESOURCEA(135)));
            if (allowDarkModeForApp)
                allowDarkModeForApp(TRUE);
        }
    }
    const HRESULT hr = DwmSetWindowAttribute(window, immersiveDarkAttribute(), &dark, sizeof(dark));
    if (theme)
    {
        using FlushMenuThemes = void(WINAPI*)();
        static const auto flushMenuThemes = reinterpret_cast<FlushMenuThemes>(
            GetProcAddress(theme, MAKEINTRESOURCEA(136)));
        if (flushMenuThemes)
            flushMenuThemes();
    }
    return hr;
}
QString failure(const char* operation, HRESULT result)
{
    return QStringLiteral("%1 failed (HRESULT 0x%2).")
        .arg(QString::fromLatin1(operation), QString::number(quint32(result), 16));
}
#if WINDOWHELER_ENABLE_LEGACY_ACCENT
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
bool accent(HWND window, int state)
{
    using SetComposition       = BOOL(WINAPI*)(HWND, CompositionData*);
    static auto setComposition = reinterpret_cast<SetComposition>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (!setComposition)
        return false;
    // Only legacy Blur uses Accent; Acrylic shares Mica's system-backdrop gate.
    AccentPolicy policy{ state, 0, 0, 0 };
    CompositionData data{ 19, &policy, sizeof(policy) };
    return setComposition(window, &data);
}
#endif
} // namespace
bool WindowsMaterial::usesSystemBackdrop(Material material)
{
    return windowsBuild() >= 22621 &&
           (material == Material::Mica || material == Material::MicaAlt || material == Material::Acrylic);
}
void WindowsMaterial::bind(HWND window)
{
    m_window       = window;
    m_haveBackdrop = SUCCEEDED(
        DwmGetWindowAttribute(window, systemBackdropAttribute, &m_originalBackdrop, sizeof(m_originalBackdrop)));
    m_haveDark =
        SUCCEEDED(DwmGetWindowAttribute(window, immersiveDarkAttribute(), &m_originalDark, sizeof(m_originalDark)));
    m_valid = false;
    // Match Windows10BorderHandler::setupNecessaryAttributes(): prepare the
    // native border at HWND binding, before the first show/material/popup.
    if (m_haveDark && SUCCEEDED(setDarkAppearance(window, TRUE)))
    {
        m_darkChanged = true;
        m_lastDark    = TRUE;
    }
}
bool WindowsMaterial::clear(QString& error)
{
    bool ok = true;
#if WINDOWHELER_ENABLE_LEGACY_ACCENT
    if (m_accentChanged)
    {
        if (accent(m_window, 0))
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
    // A partial clear is not Solid. Keep the last confirmed material and the
    // failed ownership flags for retry; successful clears remain recorded above.
    if (ok)
        m_applied = Material::Solid;
    m_valid = false;
    return ok;
}
bool WindowsMaterial::restoreDark(QString& error)
{
    if (!m_darkChanged || !m_haveDark)
        return true;
    BOOL currentDark = FALSE;
    if (FAILED(DwmGetWindowAttribute(m_window, immersiveDarkAttribute(), &currentDark, sizeof(currentDark))))
    {
        error = QStringLiteral("Cannot query the owned DWM dark appearance for restoration.");
        return false;
    }
    // Leave a later application-owned appearance change untouched.
    if (currentDark == m_lastDark)
    {
        const HRESULT hr =
            DwmSetWindowAttribute(m_window, immersiveDarkAttribute(), &m_originalDark, sizeof(m_originalDark));
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
                                   "transparency policy.");
            return false;
        }
    }
    if (m_valid && m_applied == material && m_color == color)
        return true;
    if (!clear(error))
        return false;
    // Solid, Transparent and Blur share qwindowkit's dark native border.
    // Modern backdrops still follow the host color to preserve light Mica/Acrylic.
    const BOOL dark = !usesSystemBackdrop(material) || color.lightnessF() < 0.5;
    if (m_haveDark && SUCCEEDED(setDarkAppearance(m_window, dark)))
    {
        m_darkChanged = true;
        m_lastDark    = dark;
    }
    if (material != Material::Solid)
    {
        if (material == Material::Mica || material == Material::MicaAlt || material == Material::Acrylic)
        {
            if (windowsBuild() < 22621)
            {
                error = QStringLiteral("%1 requires Windows 11 22H2 (build 22621) or later.")
                            .arg(material == Material::Acrylic ? QStringLiteral("Acrylic")
                                                               : QStringLiteral("Mica/MicaAlt"));
                return false;
            }
            // DWMSBT_TRANSIENTWINDOW (3) is Desktop Acrylic, as in qwindowkit.
            // Like Mica, unsupported systems fall back to the host's Solid background.
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
        else if (material == Material::Blur)
        {
#if WINDOWHELER_ENABLE_LEGACY_ACCENT
            if (windowsBuild() < 10240 || !accent(m_window, 3))
            {
                error = QStringLiteral("Legacy Accent Blur is unavailable or rejected by Windows.");
                return false;
            }
            m_accentChanged = true;
#else
            error = QStringLiteral("Blur requires WINDOWHELER_ENABLE_LEGACY_ACCENT=ON.");
            return false;
#endif
        }
        // Only plain transparency needs this alpha-composition request.
        // Match Qt's off-client alpha region, not a zero-sized region (opaque on Win10).
        // Do not submit this region on top of Accent's full-window
        // blur; system backdrops likewise use their own DWM composition path.
        if (material == Material::Transparent)
        {
            DWM_BLURBEHIND blur{};
            blur.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
            blur.fEnable  = TRUE;
            blur.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
            if (!blur.hRgnBlur)
            {
                error = QStringLiteral("Cannot allocate the DWM alpha region.");
                return false;
            }
            const HRESULT hr = DwmEnableBlurBehindWindow(m_window, &blur);
            DeleteObject(blur.hRgnBlur);
            if (FAILED(hr))
            {
                error = failure("DwmEnableBlurBehindWindow(alpha)", hr);
                return false;
            }
            m_blurChanged = true;
        }
    }
    m_applied = material;
    m_color   = color;
    m_valid   = true;
    return true;
}
} // namespace WindowHeler::Internal
