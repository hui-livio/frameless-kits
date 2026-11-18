> 当前实现状态：公共 API、Widgets / QML example、Windows / Cocoa 原生后端与材质适配已接入。平台支持和验证边界请见 [README](../README.md) 与 [架构落地说明](architecture.md)；视觉效果以对应系统的 example 实机验收为准。

# FramelessKit 接口使用 Demo

这是一组用于评审公共接口的调用方代码，基于设计稿 v0.5。目标是 Qt 6.8+、C++20、Widgets/QML，以及静态/动态两种库交付。

Initialization.h、WidgetAgent.h、QML WindowAgent、Material 枚举和相关信号已实现。示例可直接构建运行；不支持的原生材质明确降级，不用模拟效果冒充系统效果。

## 文件

- `WidgetsDemo.cpp`：完整的 Widgets main、标题栏、搜索框、窗口按钮、材质选择和错误显示。
- `Main.qml`：同一界面的 QML 版本，使用标准 Qt Quick Controls 按钮。
- `QuickMain.cpp`：QML 应用启动入口，在加载 QML 前调用统一初始化。

两种 UI 都是示例；真正要评审的是下方的 Agent 接入部分。QuickMain.cpp 通过 qt_add_qml_module 将 Main.qml 打包到 URI 为 FramelessDemo 的应用模块中，Main.qml 再导入 FramelessKit 库模块。

## main 初始化约定

统一公共入口声明为：

```cpp
namespace FramelessKit {
    void initialize(bool useQML = false);
}
```

Widgets：

```cpp
QApplication app(argc, argv);
FramelessKit::initialize(); // 等价于 initialize(false)
// 接着创建窗口和 WidgetAgent。
```

QML：

```cpp
QGuiApplication app(argc, argv);
FramelessKit::initialize(true);
QQmlApplicationEngine engine;
```

统一约定：在 QApplication/QGuiApplication 构造后、创建业务窗口/Agent 和 QQmlApplicationEngine 之前，由 GUI 主线程调用。useQML=false 初始化公共部分；true 同时准备 Quick 所需的 Alpha 默认值。应用无需再直接调用 QQuickWindow::setDefaultAlphaBuffer(true)。

Widgets 与 QML 混用时，构造 QApplication，然后调用 initialize(true)。useQML 表示是否启用附加的 QML/Quick 初始化，不表示禁用 Widgets，也不替应用创建 application 对象或 QML engine。

相同配置重复调用不产生重复初始化；false→true 只补充 Quick 初始化，必须早于首个 QQuickWindow 创建；true→false 不关闭已经启用的 Quick 能力。首次调用位置错误或请求了未链接的 Quick 支持时，明确报告初始化错误，不静默当作成功。

Qt 6.8 官方 QQuickWindow 文档要求：预期创建透明窗口时，在首个 QQuickWindow 创建前开启默认 Alpha 缓冲。该前置操作由 initialize(true) 统一封装；窗口内的 Agent 仍只管理单窗口的背景、材质和交互。

QML 类型注册仍走标准模块构建。动态插件加载和静态插件导入由交付配置负责；统一初始化不能补救漏链接的静态插件或缺失的模块文件。

## 常用接入

Widgets 在首次 show 前配置：

```cpp
auto *frame = new FramelessKit::WidgetAgent(window);
frame->setTitleBar(titleBar);
frame->setMaterial(FramelessKit::Material::Mica);
```

QML 在自己的 Window 内声明：

```qml
WindowAgent {
    window: root
    titleBar: titleBarItem
    material: WindowAgent.Mica
}
```

只需要普通无边框时，省略 material，默认 Solid。

## 带输入框和窗口按钮

Widgets 增加两行：

```cpp
frame->setInteractive(titleControls);
frame->setMaximizeButton(maximizeButton);
```

QML 对应增加两个属性：

```qml
interactiveItems: [titleControls]
maximizeButton: maximizeButtonItem
```

示例把搜索框、最小化、最大化和关闭按钮放在同一个 titleControls 容器，只注册这个容器即可保留整块区域的交互。不需要逐个排除三个按钮；最大化按钮单独注册的目的是 Snap Layout，不是重复排除拖动。

在此契约中，setMaximizeButton / maximizeButton 只负责输入适配与 Snap 命中，不自动再连接一个最大化动作。因此示例保留普通 Qt clicked / onClicked：最小化调用 showMinimized，最大化/还原调用 showMaximized / showNormal，关闭调用 close。

## Linux 窗口菜单

在 X11/XWayland 和 Wayland 上，标题栏右键与 Alt+Space 会请求桌面环境显示系统窗口菜单。X11 需要窗口管理器在 `_NET_SUPPORTED` 中公布 `_GTK_SHOW_WINDOW_MENU`；Wayland 需要当前鼠标/键盘输入事件提供有效 seat 与 serial，且合成器的 xdg-shell 支持该请求。库会在不支持时发出 `errorOccurred` 诊断，并保留 Qt 移动/缩放回退。

系统菜单内容和是否显示由桌面环境决定；请求已发送不保证窗口管理器会绘制菜单。构建 Linux 版本时若 Qt 缺少相应平台原生接口、XCB 头文件或与当前 Qt 完全匹配的 `Qt6::GuiPrivate` / Wayland client 头文件，对应菜单适配会关闭，其他平台和基本窗口拖动/缩放仍可构建。

```bash
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --parallel
QT_QPA_PLATFORM=xcb ./build-linux/bin/framelesskit_widgets_demo
QT_QPA_PLATFORM=wayland ./build-linux/bin/framelesskit_quick_demo
```

`FRAMELESSKIT_ENABLE_X11` 与 `FRAMELESSKIT_ENABLE_WAYLAND` 默认开启，可分别设为 `OFF`。源码支持 Qt 6.8+；启用 Wayland 适配后，二进制安装包按构建时的 Qt 精确版本匹配，因为内部使用了 Qt 的 QPA / 高 DPI 私有接口。这些接口不暴露到公共头文件，安装消费者也不需要私有开发头文件。

Linux 的 Solid / Transparent 与移动、缩放、最大化/还原共用原有 Agent API；没有增加公共后端选择接口。模糊、亚克力和云母继续按不支持回退，不模拟全局模糊或阴影。WSLg 未必提供可见系统菜单，在 KDE/GNOME 等实际桌面上的交互表现仍需对应环境验收。

## 必要接口清单

| 用途 | Widgets | QML | 是否必需 |
|---|---|---|---|
| 全局初始化 | initialize() | initialize(true) | 必需，在 main 中先执行 |
| 绑定窗口 | WidgetAgent(window) | window | 必需 |
| 指定标题栏 | setTitleBar | titleBar | 需要标题栏拖动时 |
| 选择材质 | setMaterial | material | 可选，默认 Solid |
| 保留区域交互 | setInteractive | interactiveItems | 标题栏含交互控件时 |
| 最大化按钮 Snap | setMaximizeButton | maximizeButton | 需要 Snap 时 |

只读 effectiveMaterial 用于查询已配置结果；errorOccurred(message) 用于可选诊断。Demo 为了显示失败原因订阅错误信号，应用不必照搬。

公共生命周期入口只有 main 中的 initialize；没有逐窗口 setup、prepare、ready、SurfaceMode、后端选择、缩放边框参数、单独主题配置或重复的窗口控制函数。

## 期望行为与边界

1. 标题栏左侧文字和空白区域可拖动，标题栏搜索框和按钮可正常使用。
2. 最大化按钮的正常点击由原有 Qt 逻辑执行一次；模块负责系统命中和事件桥接，不能再触发第二次窗口命令。
3. 应用先调用 initialize；useQML=true 时由模块准备 Quick 的全局 Alpha 默认值。单窗口背景、系统材质和纯色降级由 Agent 负责。Demo 不直接设置 Alpha 默认值、FramelessWindowHint、WA_TranslucentBackground 或 Window.color。
4. 使用材质时，应用不要同时绑定/绘制冲突的宿主背景；不透明的业务面板仍会遮挡后面的材质。
5. 材质下拉框表达请求，不保证所有系统都支持选项。模块在首次创建前预留 alpha；已存在的不透明表面不强制重建，而是降级并报告原因。使用 `--material=Transparent` 可验证初始透明请求；`--cycle-materials --smoke-test` 可快速检查切换与退出。
6. Windows 原生边框、阴影与基本窗口命令已有 example 验证；材质实际观感、Windows 11 Snap 以及 macOS / Linux 行为仍需相应系统实机验证。编译/启动成功不等于效果验收。

## 接口评审重点

- WidgetAgent 构造时绑定，是否足以替代独立 setup？本稿选择是。
- 一个交互容器是否可减少重复配置？Demo 展示整块排除，代价是容器内部空隙也不可拖动。
- 最大化按钮是否应自动执行窗口命令？本稿选择保留应用已有点击逻辑，避免同时出现隐式命令和业务连接。
- 材质只提供一个枚举，是否满足当前使用方式？本稿保持互斥选择，不增加每种效果的布尔开关。

Qt 普通控件接口已按 Qt 6.8 文档核对；自定义 FramelessKit 接口是本项目设计，不是 Qt 自带 API。QML 格式/语法检查不代表模块导入或运行测试通过。
