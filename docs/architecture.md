# 架构落地说明

本文件记录当前实现与设计目标的差异。原设计稿不变；目前已实现 Windows 无边框行为，并接入待 Mac 实机验证的 Cocoa 后端，窗口材质已接入后端与示例，平台观感仍需实机验收。

## 分层

```text
include/FramelessKit/    公开初始化、Material、WidgetAgent、WindowAgent
src/core/               单一运行时、窗口绑定、命中模型、材质状态、平台工厂
src/widgets/            QWidget 宿主映射与 Agent PIMPL
src/quick/              QQuickItem 映射、QML 声明收集、Quick 初始化适配与链接引导
cmake/                  target 设置、Config 模板
example/                两个已接通的架构演示
tests/                  逻辑与生命周期测试、独立安装消费工程
```

`FramelessKit::Widgets` → `FramelessKit::Core`、`Qt6::Widgets`；`FramelessKit::Quick` → `FramelessKit::Core`、`Qt6::Qml`、`Qt6::Quick`。Core 只依赖 Qt Core/Gui。按组件导出 CMake targets，Widgets-only 消费不会额外查找或链接 Quick。

## 导出与链接配置

各库统一包含源码中的 `FramelessKit/Export.h`，不再生成 `CoreExport.h` / `WidgetsExport.h` / `QuickExport.h`。默认动态库：无宏时导入；构建某个 DLL 时，仅向该 target 私有传递对应的 `FRAMELESSKIT_BUILD_CORE`、`FRAMELESSKIT_BUILD_WIDGETS` 或 `FRAMELESSKIT_BUILD_QUICK`。静态库及直接源码集成使用 `FRAMELESSKIT_STATIC` 关闭导入/导出修饰。CMake 只负责传递编译宏和安装源码头文件。

## 内部责任

- **Runtime**：检查 QGuiApplication 和 GUI 线程，维护公共/Quick 初始化状态；重复成功调用幂等，false→true 只补 Quick，true→false 不撤销。首次公共初始化不能晚于窗口创建；补 Quick 初始化不能晚于首个 QQuickWindow。
- **WindowController**：独占逻辑绑定注册表、重复绑定保护、统一错误通知、弱注册对象；只接受本窗口内的对象。销毁解除逻辑绑定，不创建原生句柄。
- **HostAdapter**：宿主对象、既有 QWindow 查询、对象归属、逻辑坐标命中、全屏状态。QWidget 和 QQuickItem 的映射分别实现；另负责首次创建前预留 alpha、验证既有表面与保存/恢复宿主背景，不隐式重建表面。
- **HitTestModel**：全屏优先；最大化按钮、交互区、标题栏剩余区域、客户区优先级。每次查询复查对象归属和可见区域，销毁或跨窗口移动的对象不能继续充当拖动区。缩放边缘由平台后端优先判断；Windows 后端处理模态输入与原生按钮序列。
- **MaterialController**：保留 requested / applied / pending。无原生表面时等待重放；非 Solid 先验证宿主 alpha，再由后端配置系统效果；成功后让宿主背景透明，失败时清理本模块的效果并恢复背景，实际配置回到 Solid，保留原请求。
- **PlatformBackend**：原生操作边界。Windows 后端挂接 HWND 消息并保留 DWM 原生边框；macOS Cocoa 后端管理 NSView/NSWindow，保留系统阴影、原生按钮和全屏控制；Linux 及非原生平台插件使用 Qt 回退。

## 统一初始化的链接方式

Core 实现唯一 `initialize(bool)`，不引用 QtQuick。Quick 模块将 `src/quick/QuickBootstrap.cpp` 预编译为 OBJECT target，由 Quick target 向消费者传播其目标文件并直接参与链接。该目标文件引用 Quick 的初始化函数并在 main 前登记至 Core。这样静态链接不会裁掉初始化适配，动态库也会在 main 前成为实际依赖；不依赖 QML 插件在加载 engine 后才登记初始化。

安装包不附带实现源码；引导目标文件安装到 `${CMAKE_INSTALL_LIBDIR}/FramelessKit/objects`，由导出的 CMake target 自动引用。它不是应用公共 API，也不要求应用手动包含内部头文件或添加链接选项。Quick 全局默认 Alpha buffer 与单窗口的 alpha 预留是渲染前置准备；窗口 Agent 只管理自身的背景和原生材质，不改全局图形后端。

## 生命周期边界

Widgets Agent 以宿主为 QObject 父对象，PIMPL 拥有 Controller。QML Agent 由 QML 对象树拥有，通过 QQmlParserStatus 在组件完成阶段建立绑定，配置顺序不影响初始请求；window 改变时重建逻辑 Controller。注册 UI 对象只保存 QPointer，不接管所有权。

Windows 与 Cocoa 后端监听 surface 生命周期，并在原生窗口可用时重建绑定；generation 用于拒绝过期的原生回调。Cocoa 通过 NSView.window 观察器跟踪 NSWindow 变化，对绑定对象安装独立的运行时子类以维持标题栏设置；不替换 Qt 的窗口 delegate，也不修改未绑定窗口的类。解除绑定时移除观察器、清理关联状态并恢复模块管理的原生属性。模块仍按进程生命周期加载，不支持任意热卸载。材质请求在新表面/新原生窗口就绪后重放；主题、调色板和系统透明策略变化触发重新配置。Quick 对已确认的 alpha 状态按表面生命周期失效，避免把旧表面的能力误用于新表面。

## 待验证与后续工作

- Cocoa 后端只在 Darwin 构建，使用 Objective-C++20 / ARC 和 AppKit；尚无 Mac 编译与 example 运行证据。
- Windows example 的 DWM 边框/阴影与基本窗口命令已获用户确认，Windows 11 Snap 弹层及跨屏 DPI 仍待验证。
- Linux 仍是 Qt 回退，尚未接入 X11/Wayland 专用菜单与阴影。
- 按当前用户要求，以 Widgets / QML example 的实际效果验收，不新增 UT。
- Windows 10 原生 example 的材质切换/降级可运行；Windows 11 系统 backdrop、Cocoa 材质及 Linux 透明仍待平台视觉验收。

## 材质后端边界

- `WindowsMaterial` 私有辅助类封装 DWM backdrop、alpha 合成和可编译关闭的 Accent ABI。WindowsBackend 仍独立负责边框与命中，现代材质只从左侧延伸 DWM frame，避免重新露出系统标题栏。解绑时恢复可读取且仍由模块持有的 backdrop / dark appearance；原生表面销毁期间只释放绑定状态。
- Cocoa 使用透明 NSWindow；Blur 在 Qt 视图下方插入 NSVisualEffectView，不替换 Qt contentView/delegate，不覆盖控件。不支持的 Windows 专属材质及“减少透明度”降级。效果视图、辅助功能观察器随窗口解绑释放。此分支尚未在 macOS 编译。
- Qt 回退只接受 Solid / Transparent，后者仍要求宿主 alpha。没有通过假模糊或全局透明度模拟 Linux 原生模糊；不承诺 Wayland / X11 合成器一定显示透明。
- HostAdapter 的透明背景接管是可逆的，不覆盖任意自定义绘制。失败后 `effectiveMaterial == Solid` 表示回到宿主背景，不意味着库会强制给原本透明的应用绘制纯色。
- 材质应用保护同步背景通知中的销毁及请求重入；切换过程中出现的新请求在当前调用结束后排队重放。错误通知排队发送，防止原生处理链被业务槽函数直接拆毁。
