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
        const QPointer<PlatformBackend> lifetime(&backend);
        if (candidate == Material::Solid || host.materialSurfaceAvailable(error))
        {
            if (candidate != Material::Solid)
            {
                // Native backdrop must see a transparent Qt host when configured.
                // A colorChanged handler may destroy/rebind the Agent synchronously.
                host.setMaterialBackground(true);
                if (!lifetime)
                    return true;
            }
            const bool configured = backend.applyMaterial(candidate, error);
            if (!lifetime)
                return true;
            if (configured)
            {
                applied = candidate;
                if (candidate == Material::Solid)
                    host.setMaterialBackground(false);
                return true;
            }
        }
        QString fallbackError;
        backend.applyMaterial(Material::Solid, fallbackError);
        if (!lifetime)
            return false;
        applied = Material::Solid;
        if (!fallbackError.isEmpty())
            error += QStringLiteral(" Fallback cleanup: ") + fallbackError;
        host.setMaterialBackground(false);
        return false;
    }
};
} // namespace FramelessKit::Internal
