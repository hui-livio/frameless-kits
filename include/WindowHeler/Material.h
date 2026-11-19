#pragma once
#include <WindowHeler/Export.h>
#include <QObject>

namespace WindowHeler
{
Q_NAMESPACE_EXPORT(WINDOWHELER_RUNTIME_EXPORT)
/// @brief 窗口材质类型
enum class Material : quint8
{
    Solid,       // 纯色
    Transparent, // 透明
    Blur,        // 毛玻璃
    Acrylic,     // 亚克力
    Mica,        // 云母
    MicaAlt      // 云母 Alt
};
Q_ENUM_NS(Material)
} // namespace WindowHeler
Q_DECLARE_METATYPE(WindowHeler::Material)
