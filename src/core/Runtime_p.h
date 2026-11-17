#pragma once
#include <FramelessKit/Export.h>
#include <QString>

namespace FramelessKit::Internal
{
// 初始化回调
using QuickInitializer = bool (*)(QString& error);
// 注册初始化回调函数
FRAMELESSKIT_CORE_EXPORT void registerQuickInitializer(QuickInitializer initializer);
// 是否已初始化
FRAMELESSKIT_CORE_EXPORT bool isInitialized(bool quick = false);
// 检查当前线程是否为 GUI 线程
FRAMELESSKIT_CORE_EXPORT bool checkGuiThread(const char* operation);
} // namespace FramelessKit::Internal
