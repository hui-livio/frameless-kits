#include "PlatformBackend_p.h"
namespace FramelessKit::Internal
{
namespace
{
class QtFallbackBackend final : public PlatformBackend
{
public:
    bool applyMaterial(Material material, QString& error) override
    {
        if (material == Material::Solid)
            return true;
        error = QStringLiteral("Material rendering is deferred in the "
                               "architecture scaffold; using Solid.");
        return false;
    }
};
} // namespace
std::unique_ptr<PlatformBackend> createQtFallbackBackend()
{
    return std::make_unique<QtFallbackBackend>();
}
#ifndef Q_OS_WIN
std::unique_ptr<PlatformBackend> createPlatformBackend(HostAdapter&)
{
    return createQtFallbackBackend();
}
#endif
} // namespace FramelessKit::Internal
