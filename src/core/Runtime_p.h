#pragma once
#include <WindowHeler/Export.h>
#include <QString>

namespace WindowHeler::Internal
{
// 初始化回调
using QuickInitializer = bool (*)(QString& error);
// 注册初始化回调函数
WINDOWHELER_CORE_EXPORT void registerQuickInitializer(QuickInitializer initializer);
// 是否已初始化
WINDOWHELER_CORE_EXPORT bool isInitialized(bool quick = false);
// 检查当前线程是否为 GUI 线程
WINDOWHELER_CORE_EXPORT bool checkGuiThread(const char* operation);
} // namespace WindowHeler::Internal
