# FramelessKit

基于 Qt 6.8+、C++20 的无边框窗口模块，提供 Widgets 与 QML 两种接入方式。

## 当前范围

| 平台 | 当前实现 | 验证状态 |
| --- | --- | --- |
| Windows | 隐藏系统标题栏、保留 DWM 边框/阴影、命中/缩放、系统菜单、最大化按钮输入桥接 | 两个 example 编译通过；用户已确认阴影及最小化、最大化/还原、关闭正常；Windows 11 Snap 弹层尚未验证 |
| macOS | Cocoa 原生窗口、全尺寸内容区、系统阴影与红黄绿按钮、窗口重建及解绑恢复 | 已接入源码与构建分支，尚未在 Mac 编译或运行 |
| Linux | Qt 公共 API 的无边框拖动/缩放回退 | 尚无专用阴影实现，X11/Wayland 菜单对齐待完成 |

材质不在本轮实现范围。`material` 保留请求，所有非 `Solid` 请求报告诊断并保持 `effectiveMaterial == Solid`；默认 `Solid` 不接管应用背景。

## 构建（不使用 Presets）

要求 CMake 3.24+，Qt 6.8+ 及其兼容 C++20 编译器。Windows 请在对应 MSVC 开发者终端执行；Qt 的路径请按本机安装修改。默认构建动态库（`FRAMELESSKIT_BUILD_SHARED_LIBS=ON`），静态/动态使用独立构建和安装目录。

```powershell
cmake -S . -B build/static -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DBUILD_SHARED_LIBS=OFF
cmake --build build/static
cmake --install build/static --prefix install/static

cmake -S . -B build/shared -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DBUILD_SHARED_LIBS=ON
cmake --build build/shared
cmake --install build/shared --prefix install/shared
```

也可使用 Visual Studio 多配置生成器（本机验证使用此方式）：

```powershell
cmake -S . -B build/vs-static -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DBUILD_SHARED_LIBS=OFF
cmake --build build/vs-static --config Debug --parallel 8
cmake --install build/vs-static --config Debug --prefix install/vs-static
```

当前建议每个构建/安装目录只发布一种配置；多配置生成器的 QML 插件与 `qmldir` 并列输出，切换 Debug/Release 会覆盖该目录下的插件。需要并存时请再使用不同的目录。

测试默认关闭，常规构建只生成库和 example。需要现有测试时，显式设置 `-DFRAMELESSKIT_BUILD_TESTING=ON` 后再执行 `ctest --test-dir <构建目录> --output-on-failure`（多配置生成器需加 `-C Debug` 等配置参数）。已有构建目录若缓存了 `ON`，请重新配置为 `-DFRAMELESSKIT_BUILD_TESTING=OFF -DBUILD_TESTING=OFF`。

### 构建开关

| 参数 | 独立构建默认 | 子项目默认 | 作用 |
| --- | --- | --- | --- |
| `FRAMELESSKIT_BUILD_SHARED_LIBS` | `ON`¹ | `ON`¹ | 静态库 / 动态库 |
| `FRAMELESSKIT_BUILD_WIDGETS` | `ON` | `ON` | Widgets 适配器 |
| `FRAMELESSKIT_BUILD_QUICK` | `ON` | `ON` | Quick 适配器、QML 模块和插件 |
| `FRAMELESSKIT_BUILD_EXAMPLES` | `ON` | `OFF` | 示例应用 |
| `FRAMELESSKIT_BUILD_TESTING` | `OFF` | `OFF` | 现有测试与 Qt Test 依赖 |
| `FRAMELESSKIT_INSTALL` | `ON` | `OFF` | 库、公开头文件、QML 插件和 CMake 包的安装规则 |
| `FRAMELESSKIT_ENABLE_LEGACY_ACCENT` | `OFF` | `OFF` | 预留编译开关，当前不实现 Accent 效果 |

¹ 如果父工程或命令行已经设置标准 `BUILD_SHARED_LIBS`，首次配置时以它作为默认值；显式设置 `FRAMELESSKIT_BUILD_SHARED_LIBS` 可独立选择本库类型，不修改父工程的选择。

纯 Widgets：追加 `-DFRAMELESSKIT_BUILD_QUICK=OFF`。纯 Quick：追加 `-DFRAMELESSKIT_BUILD_WIDGETS=OFF`。只要 Core：同时关闭两个前端；关闭测试可避免 Qt Test 依赖。Qt Quick Controls 只由 QML 演示使用，不是库的 C++ 链接依赖。

兼容旧命令行：独立构建首次配置时，旧的 `BUILD_WIDGETS`、`BUILD_QUICK`、`BUILD_EXAMPLES`、`BUILD_TESTING`、`ENABLE_LEGACY_ACCENT` 可以提供对应新开关的默认值。新开关一旦设置或写入缓存，后续请修改 `FRAMELESSKIT_*`；作为子项目时忽略这些旧名称，避免与父工程冲突。FramelessKit 不定义全局 `BUILD_SHARED_LIBS` 选项。

### 作为其他工程的子项目

支持直接 `add_subdirectory()`，无需先安装或调用 `find_package(FramelessKit)`；目标名称与安装消费保持一致。子项目默认不添加 example、测试或安装内容，也不覆盖父工程设置的运行库、归档和 QML 输出目录。

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyApp LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup(REQUIRES 6.8)

# 普通变量即可，不必 FORCE 修改父工程或用户的缓存。
set(FRAMELESSKIT_BUILD_QUICK OFF)
# 可选：仅将 FramelessKit 编译为静态库，不影响父工程其他库。
# set(FRAMELESSKIT_BUILD_SHARED_LIBS OFF)
add_subdirectory(third_party/frameless-kits)

qt_add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FramelessKit::Widgets)
```

`add_subdirectory()` 也可使用仓库的绝对路径并提供二进制目录；通过 `FetchContent_MakeAvailable()` 引入时采用同样的子项目默认值。父工程若要启用本库安装，需在接入前显式设置 `FRAMELESSKIT_INSTALL ON`；需要现有测试时设置 `FRAMELESSKIT_BUILD_TESTING ON`，并在父工程根目录 `include(CTest)` / `enable_testing()` 以便根目录 CTest 发现测试。

Quick 接入将组件改为 Qt `Qml Quick`，链接 `FramelessKit::Quick`；静态版还需要链接 `FramelessKit::QuickPlugin` 并在 C++ 中保留 `Q_IMPORT_QML_PLUGIN(FramelessKitPlugin)`，与下方安装消费方式一致。构建树中的 QML 导入根目录可从目标读取，不依赖子项目的局部变量：

```cmake
get_target_property(fk_qml_module_dir FramelessKit::Quick QT_QML_MODULE_OUTPUT_DIRECTORY)
get_filename_component(fk_qml_import_path "${fk_qml_module_dir}" DIRECTORY)
target_compile_definitions(my_app PRIVATE
    FRAMELESSKIT_APP_QML_IMPORT_PATH="${fk_qml_import_path}"
)
```

创建 QML engine 后使用 `engine.addImportPath(QString::fromUtf8(FRAMELESSKIT_APP_QML_IMPORT_PATH))`，并在创建 engine / 窗口前调用 `FramelessKit::initialize(true)`。

Windows 动态构建时，父工程应统一设置 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`，或将依赖 DLL 复制到应用目录（Qt 平台插件仍需按正常 Qt 应用部署）：

```cmake
if(WIN32)
    add_custom_command(TARGET my_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:my_app> $<TARGET_FILE_DIR:my_app>
        COMMAND_EXPAND_LISTS
    )
endif()
```

### 统一导出头与编译宏

所有模块共用仓库内的 `include/FramelessKit/Export.h`，不再由 CMake 生成导出头；直接包含源码头文件也不需要先运行 configure。

| 编译方式 | 需要传递的宏 |
| --- | --- |
| 使用动态库（默认） | 无；默认将各模块符号标记为导入 |
| 构建 Core 动态库 | 仅在 Core 编译单元中定义 `FRAMELESSKIT_BUILD_CORE` |
| 构建 Widgets 动态库 | 仅在 Widgets 编译单元中定义 `FRAMELESSKIT_BUILD_WIDGETS` |
| 构建 Quick 动态库 | 仅在 Quick 编译单元中定义 `FRAMELESSKIT_BUILD_QUICK` |
| 构建/使用静态库，或将源码直接编入应用 | 为相关编译单元统一定义 `FRAMELESSKIT_STATIC`，关闭所有导入/导出修饰 |

三个模块仍各用 `FRAMELESSKIT_CORE_EXPORT`、`FRAMELESSKIT_WIDGETS_EXPORT`、`FRAMELESSKIT_QUICK_EXPORT` 标注接口，但定义集中于同一个头文件。不要向动态库消费者传播 `FRAMELESSKIT_BUILD_*`，也不要在编译某一个 DLL 时同时定义三个构建宏。

使用本项目 CMake targets 时，构建宏由各 DLL target 私有传递；静态 target 会公开传递 `FRAMELESSKIT_STATIC`。使用其他构建方式时，由用户按上表传递编译宏。

```cmake
# 用户工程中直接编译源码 / 链接手动构建的静态库时：
target_compile_definitions(my_app PRIVATE FRAMELESSKIT_STATIC)
```

这些宏只决定符号可见性；直接集成源码仍需正常配置 Qt AUTOMOC、QML 注册/插件与 Quick 初始化接入。
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

## macOS example 验收

macOS 自动选择 Cocoa 后端，Core 私有链接 AppKit，使用 Objective-C++20 / ARC；不需要应用引入原生头文件。非 Cocoa 平台插件（例如 offscreen）仍使用 Qt 回退。

```sh
cmake -S . -B build/macos -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/macos -DBUILD_TESTING=OFF
cmake --build build/macos --target framelesskit_widgets_demo framelesskit_quick_demo
./build/macos/bin/framelesskit_widgets_demo
./build/macos/bin/framelesskit_quick_demo
```

两个示例在 macOS 上保留系统红黄绿按钮，隐藏重复的自定义窗口按钮，并给左上角原生控件预留空间。请在 Mac 上检查：

- 自定义标题栏、系统阴影、原生按钮显示是否正常；输入框区域不能拖动窗口。
- 标题栏拖动、窗口边缘缩放，以及最小尺寸约束。
- 红黄绿按钮的关闭、最小化、恢复、进入/退出全屏；双击标题栏的系统偏好。
- 隐藏后再次显示、多个窗口分别关闭，以及 QML Agent 换绑窗口后的样式恢复。

当前仅有 Windows 环境的构建验证；上述 Mac 运行效果仍待实机验收。不以 Windows 构建成功代替 Cocoa 分支的编译或运行验证。
