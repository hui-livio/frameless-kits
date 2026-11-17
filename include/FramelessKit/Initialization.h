#pragma once
#include <FramelessKit/Export.h>

namespace FramelessKit
{
/**
 * @brief 初始化 FramelessKit 库
 * @param useQML 是否初始化 QML 引擎相关的功能
 *
 * @note 调用此函数应在 GUI 线程上，且在创建任何窗口之前
 */
FRAMELESSKIT_CORE_EXPORT void initialize(bool useQML = false);
} // namespace FramelessKit
