#pragma once
#include <FramelessKit/CoreExport.h>
#include <QString>

namespace FramelessKit::Internal
{
using QuickInitializer = bool (*)(QString& error);
FRAMELESSKIT_CORE_EXPORT void registerQuickInitializer(QuickInitializer initializer);
FRAMELESSKIT_CORE_EXPORT bool isInitialized(bool quick = false);
FRAMELESSKIT_CORE_EXPORT bool checkGuiThread(const char* operation);
} // namespace FramelessKit::Internal
