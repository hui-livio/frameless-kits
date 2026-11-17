#pragma once
#include <FramelessKit/CoreExport.h>
#include <QObject>

namespace FramelessKit
{
Q_NAMESPACE_EXPORT(FRAMELESSKIT_CORE_EXPORT)
enum class Material
{
    Solid,
    Transparent,
    Blur,
    Acrylic,
    Mica,
    MicaAlt
};
Q_ENUM_NS(Material)
} // namespace FramelessKit
Q_DECLARE_METATYPE(FramelessKit::Material)
