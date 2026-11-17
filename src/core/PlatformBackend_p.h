#pragma once
#include <FramelessKit/Material.h>
#include <QString>
#include <memory>

namespace FramelessKit::Internal
{
class HostAdapter;
// Future implementations own native bindings, frame/caption handling and
// material resources.
class PlatformBackend
{
public:
    virtual ~PlatformBackend()                                    = default;
    virtual bool applyMaterial(Material material, QString& error) = 0;
};
std::unique_ptr<PlatformBackend> createPlatformBackend(HostAdapter& host);
} // namespace FramelessKit::Internal
