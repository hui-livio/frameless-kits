#include "PlatformBackend_p.h"
namespace FramelessKit::Internal
{
// Keep the platform boundary separate. No HWND hooks or DWM state are changed
// yet.
std::unique_ptr<PlatformBackend> createQtFallbackBackend();
std::unique_ptr<PlatformBackend> createPlatformBackend(HostAdapter&)
{
    return createQtFallbackBackend();
}
} // namespace FramelessKit::Internal
