# FramelessKit

基于 Qt 6.8+、C++20 的无边框窗口模块。当前交付为**架构骨架**，不是窗口效果实现。

## 当前范围

| 已实现 | 暂缓实现 |
| --- | --- |
| Core / Widgets / Quick 三库及命名空间 target | Windows 原生消息链、客户区和 HWND 重建 |
| main 统一初始化、Quick Alpha 初始化接入 | 无边框拖动、八方向缩放、系统菜单 |
| Agent PIMPL、重复绑定保护、弱引用、对象归属校验 | Snap Layout 与原生按钮输入桥接 |
| 内部 HostAdapter、HitTestModel、MaterialController、PlatformBackend | 像素透明、Blur、Acrylic、Mica / Mica Alt |
| 标准 QML 模块、静态插件、安装与按组件消费 | 材质视觉、DPI、任务栏和多平台实机验收 |

窗口目前保留系统边框，`titleBar` / `maximizeButton` 只保存配置，不触发原生行为。`material` 保留请求，所有非 `Solid` 请求报告诊断并保持 `effectiveMaterial == Solid`；不会把占位后端标成材质成功。默认 `Solid` 不接管或修改应用背景。

## 构建（不使用 Presets）

要求 CMake 3.24+，Qt 6.8+ 及其兼容 C++20 编译器。Windows 请在对应 MSVC 开发者终端执行；Qt 的路径请按本机安装修改。静态/动态使用独立构建和安装目录。

```powershell
cmake -S . -B build/static -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DBUILD_SHARED_LIBS=OFF
cmake --build build/static
ctest --test-dir build/static --output-on-failure
cmake --install build/static --prefix install/static

cmake -S . -B build/shared -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DBUILD_SHARED_LIBS=ON
cmake --build build/shared
ctest --test-dir build/shared --output-on-failure
cmake --install build/shared --prefix install/shared
```

也可使用 Visual Studio 多配置生成器（本机验证使用此方式）：

```powershell
cmake -S . -B build/vs-static -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DBUILD_SHARED_LIBS=OFF
cmake --build build/vs-static --config Debug --parallel 8
ctest --test-dir build/vs-static -C Debug --output-on-failure
cmake --install build/vs-static --config Debug --prefix install/vs-static
```

当前建议每个构建/安装目录只发布一种配置；多配置生成器的 QML 插件与 `qmldir` 并列输出，切换 Debug/Release 会覆盖该目录下的插件。需要并存时请再使用不同的目录。

### 构建开关

| 参数 | 默认 | 作用 |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `OFF` | 静态库 / 动态库，独立构建 |
| `BUILD_WIDGETS` | `ON` | Widgets 适配器与对应示例/测试 |
| `BUILD_QUICK` | `ON` | Quick 适配器、QML 模块和插件 |
| `BUILD_EXAMPLES` | `ON` | 两个架构演示 |
| `BUILD_TESTING` | `ON` | Qt Test、示例 smoke、安装消费测试 |
| `ENABLE_LEGACY_ACCENT` | `OFF` | 预留编译开关，当前不实现 Accent 效果 |

纯 Widgets：追加 `-DBUILD_QUICK=OFF`。纯 Quick：追加 `-DBUILD_WIDGETS=OFF`。只要 Core：同时关闭两个前端；关闭测试可避免 Qt Test 依赖。Qt Quick Controls 只由 QML 演示使用，不是库的 C++ 链接依赖。

## 安装后使用

通过 `CMAKE_PREFIX_PATH` 或 `FramelessKit_DIR` 指向安装目录。仅公开头文件会安装；`src/` 中的 Controller、HostAdapter 等内部头文件不会安装。

```cmake
find_package(FramelessKit 0.1 CONFIG REQUIRED COMPONENTS Widgets)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FramelessKit::Widgets)
```

```cpp
#include <FramelessKit/Initialization.h>
#include <FramelessKit/WidgetAgent.h>

QApplication app(argc, argv);
FramelessKit::initialize();
QWidget window;
auto *agent = new FramelessKit::WidgetAgent(&window);
agent->setTitleBar(titleBar); // titleBar 是本窗口中的 QWidget
```

Quick / QML：

```cmake
find_package(FramelessKit 0.1 CONFIG REQUIRED COMPONENTS Quick)
add_executable(my_qml_app main.cpp)
target_link_libraries(my_qml_app PRIVATE FramelessKit::Quick)
get_target_property(quick_type FramelessKit::Quick TYPE)
if(quick_type STREQUAL "STATIC_LIBRARY")
    target_link_libraries(my_qml_app PRIVATE FramelessKit::QuickPlugin)
endif()
```

`main.cpp` 中在创建 engine / 窗口前调用 `FramelessKit::initialize(true)`。静态包需保留插件入口：

```cpp
#include <QtQml/QQmlExtensionPlugin>
#ifdef FRAMELESSKIT_STATIC
Q_IMPORT_QML_PLUGIN(FramelessKitPlugin)
#endif
```

构建 target 自动接入 Quick 初始化适配，不需要应用手工注册回调；纯 Widgets / Core 的二进制不链接 QtQuick。QML 使用 `import FramelessKit`、`WindowAgent` 及 `WindowAgent.Solid` 等枚举。安装后的 QML 搜索根目录由 `FramelessKit_QML_IMPORT_PATH` 给出，加载 engine 时使用 `addImportPath`，或按应用部署方式设置导入路径。

动态包还需要部署 `bin` 中的库、`lib/qml/FramelessKit` 中的插件/元数据以及匹配的 Qt 运行库。模块静态不等于 Qt 静态。本工程不承诺运行中热卸载或同时加载不同静态/动态版本。

## 示例与测试

- `framelesskit_widgets_demo`：Widgets 最小 API、交互区域和材质请求。
- `framelesskit_quick_demo`：QML 注册、声明式配置和材质请求。
- `ctest`：核心命中优先级、初始化、生命周期、弱引用、属性/列表、两个示例启动、QML 加载，以及安装后 Widgets / Quick / 混合应用的独立 configure → build → run。

CTest 自动添加匹配 Qt 的 DLL 路径，并使用 `offscreen` 平台，因此测试不要求展示窗口。手动启动示例时，请将对应 Qt `bin` 加入 PATH（或使用 `windeployqt` 部署）；动态版还需保证本模块 DLL 可加载。自动测试不代表视觉效果或 Snap 已验收。

详细边界见 [架构落地说明](docs/architecture.md)；原设计文档是后续实现的目标，不代表当前全部能力已经完成。

### 运行本机已构建的示例

```powershell
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
.\build\static\bin\Debug\framelesskit_widgets_demo.exe
.\build\static\bin\Debug\framelesskit_quick_demo.exe
```

两个示例都支持 `--smoke-test`，用于创建窗口、接通 API、进入事件循环后自动退出；这只是运行检查，不进行视觉效果验收。
