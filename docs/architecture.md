# 架构落地说明

本次按“先搭架构，效果实现暂缓”收敛设计文档。未修改原设计稿；本文件说明当前可编译实现与设计目标的差异。

## 分层

```text
include/FramelessKit/    公开初始化、Material、WidgetAgent、WindowAgent
src/core/               单一运行时、窗口绑定、命中模型、材质状态、平台工厂
src/widgets/            QWidget 宿主映射与 Agent PIMPL
src/quick/              QQuickItem 映射、QML 声明收集、Quick 初始化适配
cmake/                  target 设置、Config 模板、Quick 链接引导
example/                两个已接通的架构演示
tests/                  逻辑与生命周期测试、独立安装消费工程
```

`FramelessKit::Widgets` → `FramelessKit::Core`、`Qt6::Widgets`；`FramelessKit::Quick` → `FramelessKit::Core`、`Qt6::Qml`、`Qt6::Quick`。Core 只依赖 Qt Core/Gui。按组件导出 CMake targets，Widgets-only 消费不会额外查找或链接 Quick。

## 内部责任

- **Runtime**：检查 QGuiApplication 和 GUI 线程，维护公共/Quick 初始化状态；重复成功调用幂等，false→true 只补 Quick，true→false 不撤销。首次公共初始化不能晚于窗口创建；补 Quick 初始化不能晚于首个 QQuickWindow。
- **WindowController**：独占逻辑绑定注册表、重复绑定保护、统一错误通知、弱注册对象；只接受本窗口内的对象。销毁解除逻辑绑定，不创建原生句柄。
- **HostAdapter**：宿主对象、既有 QWindow 查询、对象归属、逻辑坐标命中、全屏状态。QWidget 和 QQuickItem 的映射分别实现；后续原生后端通过该边界扩展宿主操作。
- **HitTestModel**：全屏优先；最大化按钮、交互区、标题栏剩余区域、客户区优先级。每次查询复查对象归属和可见区域，销毁或跨窗口移动的对象不能继续充当拖动区。缩放边缘、模态输入和原生按钮序列留待平台实现。
- **MaterialController**：保留 requested / applied，统一应用失败后的 Solid 降级。当前后端仅接受 Solid。
- **PlatformBackend**：原生操作边界。Windows 工厂已独立，目前明确使用无效果的 Qt 占位回退；不会安装 WndProc subclass 或修改 DWM。

## 统一初始化的链接方式

Core 实现唯一 `initialize(bool)`，不引用 QtQuick。Quick target 向消费者传播一个很小的 `QuickBootstrap.cpp` 源文件，它引用 Quick 的初始化函数并在 main 前登记至 Core。这样静态链接不会裁掉初始化适配，动态库也会在 main 前成为实际依赖；不依赖 QML 插件在加载 engine 后才登记初始化。

此源文件随安装包放入 `lib/cmake/FramelessKit`；它不是应用公共 API，也不要求应用手动包含内部头文件。Quick 默认 Alpha buffer 是唯一已落地的渲染前置准备，不表示透明或材质已经实现。

## 生命周期边界

Widgets Agent 以宿主为 QObject 父对象，PIMPL 拥有 Controller。QML Agent 由 QML 对象树拥有，通过 QQmlParserStatus 在组件完成阶段建立绑定，配置顺序不影响初始请求；window 改变时重建逻辑 Controller。注册 UI 对象只保存 QPointer，不接管所有权。

当前只建立逻辑生命周期，没有 NativeWindowBinding、HWND generation、surface 销毁/重放、原生状态恢复和后台材质刷新。这些必须在实际后端实现时增加，并补对应自动测试与实机验证，不能将当前逻辑生命周期测试当作原生生命周期完成。

## 后续扩展顺序

1. 扩展 HostAdapter 的窗口能力、状态/尺寸约束及宿主操作；实现 Qt 公共 API 的拖动/缩放回退。
2. 原型验证 Qt 与 Windows 消息次序，再加入 NativeWindowBinding、FrameController、几何和 DPI 处理。
3. 实现表面策略、背景所有权、MaterialController 的 pending / 重放与 WindowsMaterialBackend。
4. 验证标准按钮事件序列后实现 CaptionController / Snap，避免重复触发业务 clicked 命令。
5. 增加实机材质、跨屏缩放、系统策略与视觉回归验收。

这些能力目前均未启用；`ENABLE_LEGACY_ACCENT` 仅预留编译配置。
