#include "PlatformBackend_p.h"
namespace WindowHeler::Internal
{
std::unique_ptr<PlatformBackend> createQtFallbackBackend(HostAdapter& host, const HitTestModel& hitTest,
                                                         PlatformBackend::Diagnostic diagnostic)
{
    auto backend = std::make_unique<PlatformBackend>(host, hitTest, std::move(diagnostic));
    return backend;
}
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS) && !defined(WINDOWHELER_HAS_LINUX_BACKEND)
std::unique_ptr<PlatformBackend> createPlatformBackend(HostAdapter& host, const HitTestModel& hitTest,
                                                       PlatformBackend::Diagnostic diagnostic)
{
    return createQtFallbackBackend(host, hitTest, std::move(diagnostic));
}
#endif
} // namespace WindowHeler::Internal
