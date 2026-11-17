#pragma once
#include "HostAdapter_p.h"
#include "PlatformBackend_p.h"

namespace FramelessKit::Internal
{
// Requests survive unavailable/recreated surfaces. Applied means configured,
// not a guarantee that the compositor currently displays a particular effect.
class MaterialController
{
public:
    Material requested = Material::Solid;
    Material applied   = Material::Solid;
    bool pending       = false;

    bool apply(PlatformBackend& backend, HostAdapter& host, QString& error)
    {
        pending                  = false;
        const Material candidate = requested;
        if (candidate != Material::Solid && !backend.materialReady())
        {
            pending = true;
            applied = Material::Solid;
            host.setMaterialBackground(false);
            return true;
        }
        if ((candidate == Material::Solid || host.materialSurfaceAvailable(error)) &&
            backend.applyMaterial(candidate, error))
        {
            applied = candidate;
            host.setMaterialBackground(candidate != Material::Solid);
            return true;
        }
        QString fallbackError;
        backend.applyMaterial(Material::Solid, fallbackError);
        applied = Material::Solid;
        if (!fallbackError.isEmpty())
            error += QStringLiteral(" Fallback cleanup: ") + fallbackError;
        host.setMaterialBackground(false);
        return false;
    }
};
} // namespace FramelessKit::Internal
