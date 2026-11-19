#pragma once
#include <WindowHeler/Export.h>
#include <QtGlobal>

class QObject;

namespace WindowHeler::Internal
{
struct WindowContextHandle;
enum class Capability : quint32
{
    Frameless = 1u,
    Material = 2u,
};

extern "C" {
WINDOWHELER_RUNTIME_EXPORT WindowContextHandle* windowheler_acquire_context(QObject* host);
WINDOWHELER_RUNTIME_EXPORT void windowheler_retain_capability(WindowContextHandle*, Capability capability);
WINDOWHELER_RUNTIME_EXPORT void windowheler_release_capability(WindowContextHandle*, Capability capability);
WINDOWHELER_RUNTIME_EXPORT void windowheler_release_context(WindowContextHandle*);
}
} // namespace WindowHeler::Internal
