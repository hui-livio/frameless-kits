#pragma once
#include "PlatformBackend_p.h"
namespace FramelessKit::Internal
{
// No QObject needed: notifications and diagnostics are owned by
// WindowController.
class MaterialController
{
public:
    Material requested = Material::Solid;
    Material applied   = Material::Solid;
    bool apply(PlatformBackend& backend, Material material, QString& error)
    {
        requested = material;
        if (backend.applyMaterial(material, error))
        {
            applied = material;
            return true;
        }
        QString fallbackError;
        backend.applyMaterial(Material::Solid, fallbackError);
        applied = Material::Solid;
        return false;
    }
};
} // namespace FramelessKit::Internal
