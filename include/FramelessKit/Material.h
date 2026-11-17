#pragma once
#include <FramelessKit/Export.h>
#include <QObject>

namespace FramelessKit
{
Q_NAMESPACE_EXPORT(FRAMELESSKIT_CORE_EXPORT)
/// @brief 窗口材质类型
enum class Material : quint8
{
    Solid,       // 纯色
    Transparent, // 透明
    Blur,        // 毛玻璃
    Acrylic,     // 亚克力
    Mica,        // Mica
    MicaAlt      // Mica Alt
};
Q_ENUM_NS(Material)
} // namespace FramelessKit
Q_DECLARE_METATYPE(FramelessKit::Material)
