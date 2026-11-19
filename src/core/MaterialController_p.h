#pragma once
#include "HostAdapter_p.h"
#include "PlatformBackend_p.h"

namespace WindowHeler::Internal
{
// Requests survive unavailable/recreated surfaces. Applied records the last
// confirmed configuration, not a guarantee of the currently displayed effect.
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
        const QPointer<PlatformBackend> lifetime(&backend);
        const bool ready = candidate == Material::Solid || backend.materialReady();
        if (!lifetime)
            return true;
        if (!ready)
        {
            pending = true;
            // Unavailability alone does not confirm that native cleanup succeeded.
            host.setMaterialBackground(applied != Material::Solid);
            return true;
        }
        const bool surfaceAvailable = candidate == Material::Solid || host.materialSurfaceAvailable(error);
        if (!lifetime)
            return true;
        if (surfaceAvailable)
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
        const bool cleared = backend.applyMaterial(Material::Solid, fallbackError);
        if (!lifetime)
            return false;
        if (cleared)
            applied = Material::Solid;
        else if (fallbackError.isEmpty())
            fallbackError = QStringLiteral("Cannot clear the previous native material.");
        if (error.isEmpty())
            error = QStringLiteral("Cannot apply the requested material.");
        if (!fallbackError.isEmpty())
            error += QStringLiteral(" Fallback cleanup: ") + fallbackError;
        // Failed cleanup retains the last confirmed material and its host background.
        host.setMaterialBackground(applied != Material::Solid);
        return false;
    }
};
} // namespace WindowHeler::Internal
