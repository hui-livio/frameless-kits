# 架构落地说明

本文件记录当前实现与设计目标的差异。原设计稿不变；目前已实现 Windows 无边框行为，并接入待 Mac 实机验证的 Cocoa 后端，窗口材质已接入后端与示例，平台观感仍需实机验收。

## 分层

```text
include/WindowHeler/    Runtime、初始化、Material、FramelessFramelessWidgetAgent / MaterialWidgetAgent、MaterialFramelessWidgetAgent / MaterialWidgetAgent、FramelessAgent、MaterialAgent
src/core/               单一运行时、窗口绑定、命中模型、材质状态、平台工厂
src/widgets/            QWidget 宿主映射与 Agent PIMPL
src/quick/              QQuickItem 映射、QML 声明收集、Quick 初始化适配与链接引导
cmake/                  target 设置、Config 模板
example/                两个已接通的架构演示
tests/                  逻辑与生命周期测试、独立安装消费工程
```

`WindowHeler::Runtime` 是始终 SHARED 的运行时库，持有跨 DLL 的 WindowContext 注册表、句柄和 Runtime ABI。四个前端 target `WindowHeler::FramelessWidgets`、`WindowHeler::FramelessQuick`、`WindowHeler::MaterialWidgets`、`WindowHeler::MaterialQuick` 只依赖 Runtime；前端 target 根据 `WINDOWHELER_BUILD_SHARED_LIBS` 生成 STATIC/SHARED。库文件名为 `windowhelerruntime`、`framelesswidget`、`framelessquick`、`framelessmaterialwidget`、`framelessmaterialquick`。Widgets 与 Quick 保持独立依赖，不提供 `WindowHeler::Widgets` 或 `WindowHeler::Quick` 组合 target。

## 导出与链接配置

各库统一包含源码中的 `WindowHeler/Export.h`，不再生成 `CoreExport.h` / `WidgetsExport.h` / `QuickExport.h`。默认动态库：Runtime 和各前端 DLL 分别使用对应导出宏；静态库及直接源码集成使用 `WINDOWHELER_STATIC` 关闭导入/导出修饰。前端不再嵌入 Runtime 对象文件，所有跨 DLL 状态只来自 `WindowHeler::Runtime`。

## 内部责任

- **Runtime**：检查 QGuiApplication 和 GUI 线程，维护公共/Quick 初始化状态；重复成功调用幂等，false→true 只补 Quick，true→false 不撤销。首次公共初始化不能晚于窗口创建；补 Quick 初始化不能晚于首个 QQuickWindow。
- **WindowController**：独占逻辑绑定注册表、重复绑定保护、统一错误通知、弱注册对象；只接受本窗口内的对象。销毁解除逻辑绑定，不创建原生句柄。
- **HostAdapter**：宿主对象、既有 QWindow 查询、对象归属、逻辑坐标命中、全屏状态。QWidget 和 QQuickItem 的映射分别实现；另负责首次创建前预留 alpha、验证既有表面与保存/恢复宿主背景，不隐式重建表面。
- **HitTestModel**：全屏优先；最大化按钮、交互区、标题栏剩余区域、客户区优先级。每次查询复查对象归属和可见区域，销毁或跨窗口移动的对象不能继续充当拖动区。缩放边缘由平台后端优先判断；Windows 后端处理模态输入与原生按钮序列。
- **MaterialController**：保留 requested / applied / pending。无原生表面时等待重放；非 Solid 先验证宿主 alpha，再准备透明宿主背景并配置原生效果；成功后提交 applied，失败时清理本模块的效果并恢复背景，实际配置回到 Solid，保留原请求。
- **PlatformBackend**：原生操作边界。Windows 后端挂接 HWND 消息并保留 DWM 原生边框；macOS Cocoa 后端管理 NSView/NSWindow，保留系统阴影、原生按钮和全屏控制；Linux 后端向 X11 / Wayland 合成器请求系统窗口菜单，并用 Qt 公共 API 执行移动/缩放。非原生插件保留 Qt 回退。

## 统一初始化的链接方式

Runtime 实现唯一 `initialize(bool)`，不引用 QtQuick。Quick 模块将 `src/quick/QuickBootstrap.cpp` 直接编译进 `WindowHeler::FramelessQuick`，并在 main 前登记 Quick 初始化适配到 Runtime。这样静态链接不会裁掉初始化适配，动态库也会在 main 前成为实际依赖；不依赖 QML 插件在加载 engine 后才登记初始化。

安装包不附带实现源码；引导目标文件安装到 `${CMAKE_INSTALL_LIBDIR}/WindowHeler/objects`，由导出的 CMake target 自动引用。它不是应用公共 API，也不要求应用手动包含内部头文件或添加链接选项。Quick 全局默认 Alpha buffer 与单窗口的 alpha 预留是渲染前置准备；窗口 Agent 只管理自身的背景和原生材质，不改全局图形后端。

## 生命周期边界

Widgets Agent 以宿主为 QObject 父对象，PIMPL 拥有 Controller。FramelessAgent / MaterialAgent 由 QML 对象树拥有，通过 QQmlParserStatus 在组件完成阶段建立绑定，配置顺序不影响初始请求；window 改变时重建逻辑 Controller。注册 UI 对象只保存 QPointer，不接管所有权。

Windows 与 Cocoa 后端监听 surface 生命周期，并在原生窗口可用时重建绑定；generation 用于拒绝过期的原生回调。Cocoa 通过 NSView.window 观察器跟踪 NSWindow 变化，对绑定对象安装独立的运行时子类以维持标题栏设置；不替换 Qt 的窗口 delegate，也不修改未绑定窗口的类。解除绑定时移除观察器、清理关联状态并恢复模块管理的原生属性。模块仍按进程生命周期加载，不支持任意热卸载。材质请求在新表面/新原生窗口就绪后重放；主题、调色板和系统透明策略变化触发重新配置。Quick 对已确认的 alpha 状态按表面生命周期失效，避免把旧表面的能力误用于新表面。

## 重要审查修复（2026-11-18）

- 重要审查修复：Quick 换绑按操作版本串行执行；同步回调中的新请求在当前清理/创建结束后重放，旧操作不再覆盖新窗口。绑定表保存原始身份键，宿主 QPointer 清空后也能正确注销；旧 Controller 完成清理前保留注册占用。
- 命中测试保存弱引用快照与注册版本，Quick containment mask 修改对象、注册或绑定后立即放弃本次命中。交互对象销毁时由 Core 清理注册，跨窗口后仍可主动移除。
- Quick 列表替换先一次性更新配置，再发送通知；解绑和宿主销毁补齐 effectiveMaterial 通知，换绑剪除交互项时同步断开旧连接并通知列表变化。
- Quick 已确认的 alpha 表面能力由窗口拥有的内部观察器记录，Agent 解绑后仍保留同一表面的确认结果；表面销毁时立即失效。避免恢复不透明背景后 Qt 报告 alpha 未指定，使随后重新绑定误降级；不因此重建表面。
- 材质清理失败保留上一次确认配置及待清理标记，只有 fallback 成功才更新为 Solid。此异常分支做了源码复核，未模拟原生 API 故障。默认成功路径和材质循环已通过 example 检查。
- QWidget 能力按其规范化 flags 判断，尊重显式禁用最大化；QWindow 的默认 flags 解释保持独立。安装包的 OPTIONAL_COMPONENTS 缺依赖时只标记对应组件不可用。

## 待验证与后续工作

- Cocoa 后端只在 Darwin 构建，使用 Objective-C++20 / ARC、AppKit、QuartzCore 与 CoreGraphics；尚无 Mac 编译与 example 运行证据。
- Windows example 的 DWM 边框/阴影与基本窗口命令已获用户确认，Windows 11 Snap 弹层及跨屏 DPI 仍待验证。
- Linux 系统菜单接入 X11 `_GTK_SHOW_WINDOW_MENU` 与 Wayland `xdg_toplevel.show_window_menu`；菜单可见性由 WM/compositor 决定，不对未公告能力的桌面伪造成功。移动/缩放仍由 Qt 公共 API 处理；不宣称提供 X11/Wayland 原生窗口阴影。
- 按当前用户要求，以 Widgets / QML example 的实际效果验收，不新增 UT。
- Windows 10 原生 example 的材质切换/降级可运行；Windows 11 系统 backdrop、Cocoa 材质及 Linux 透明仍待平台视觉验收。

## 材质后端边界

- `WindowsMaterial` 私有辅助类封装 DWM backdrop、alpha 合成和可编译关闭的 Accent ABI。WindowsBackend 仍独立负责边框与命中，现代材质只从左侧延伸 DWM frame，避免重新露出系统标题栏。解绑时恢复可读取且仍由模块持有的 backdrop / dark appearance；原生表面销毁期间只释放绑定状态。
- Cocoa 使用透明 NSWindow；Blur 在 Qt 视图下方插入 NSVisualEffectView，不替换 Qt contentView/delegate，不覆盖控件。不支持的 Windows 专属材质及“减少透明度”降级。效果视图、辅助功能观察器随窗口解绑释放。此分支尚未在 macOS 编译。
- Qt 回退只接受 Solid / Transparent，后者仍要求宿主 alpha。没有通过假模糊或全局透明度模拟 Linux 原生模糊；不承诺 Wayland / X11 合成器一定显示透明。
- HostAdapter 的透明背景接管是可逆的，不覆盖任意自定义绘制。失败后 `effectiveMaterial == Solid` 表示回到宿主背景，不意味着库会强制给原本透明的应用绘制纯色。
- 材质应用保护同步背景通知中的销毁及请求重入；切换过程中出现的新请求在当前调用结束后排队重放。错误通知排队发送，防止原生处理链被业务槽函数直接拆毁。

## qwindowkit 材质对照

参考本地 `qwindowkit/src/core/contexts/win32windowcontext.cpp` 的 `windowAttributeChanged()`，及 `cocoawindowcontext.mm` 的 `findBlurEffectView()` / `setBlurEffect()`，以原理对齐而非迁入其动态属性 API。

- Windows 原生效果应用前先使 Qt 宿主背景透明。Accent Blur 使用普通 margins（Win10 与普通原生边框一样保留 top 1px 扩展）；现代 backdrop 使用 `{65536, 0, 0, 0}` 从左侧覆盖客户区，既不能把 Win10 的 top 1px 扩展叠在 backdrop 上，也不对 Blur 使用全窗口扩展。Transparent 使用与 Qt 6.8.3 一致的 `CreateRectRgn(0, 0, -1, -1)` 离客户区 region；不能替换成零大小 region，后者在 Win10 上导致白底。该 DWM alpha 请求不叠加到 Accent / backdrop。
- 绑定期间统一屏蔽 `WS_SYSMENU`，对齐 qwindowkit，避免透明标题栏露出系统按钮；解绑按属性归属恢复。自定义按钮、右键系统菜单和 Alt+Space 由模块接回显式逻辑，覆盖 Solid / Transparent / Blur / 系统 backdrop。
- Quick 首次启用非 Solid 材质（包括 Transparent）时，按 qwindowkit 的一次性 resize/move 原理刷新原生合成表面；同一 HWND 仅执行一次，禁止中间重绘、保持激活状态和 Z 顺序，并在同一回调恢复原始物理窗口矩形。Widgets 不执行；最大化/最小化/全屏期间不执行，恢复普通窗口后再尝试。该 workaround 不销毁原生表面。
- Cocoa 除 NSWindow 外，还让 Qt NSView 的 CALayer 透出背景；效果视图覆盖其父容器 bounds、置于 Qt 视图下方，并采用 Active 状态和 VibrantLight / VibrantDark 外观。切回 Solid / 解绑时恢复接管的 layer 字段，CGColor 按 CoreGraphics 所有权规则释放。
- 不照搬旧 Windows 11 的私有 Mica 属性：仍按设计要求仅保证 build 22621+ 的公开 backdrop。build 22621+ 的 Acrylic 使用与 qwindowkit 相同的系统 Desktop Acrylic。按用户要求，旧系统 Acrylic 与 Mica 一样回退默认 Solid，不启用 Accent Acrylic；qwindowkit 中的注释样例不作为实际实现。Linux 仍不增加 qwindowkit 本身也未提供的通用原生模糊后端。

以上是实现路径对照，不是视觉一致性结论。Windows 11 / macOS 效果、首次 Quick 材质刷新对窗口几何的影响，仍须使用对应系统的 example 检查。

### Windows 10 实际渲染排查（2026-11-17）

- 在忽略的 build 目录使用调用公开 Agent API 的临时受控背景程序，并排查看 Widgets / Quick。零大小 DWM region 可复现两端白底；修正 region 并让 Quick 的一次性表面刷新覆盖 Transparent 后，两端均能清晰透出棋盘格与背景文字，前景文字及不透明控件仍正常。没有加入 UT 或正式构建目标。
- Legacy Acrylic 只保留 tint 标志，不照搬 qwindowkit 注释代码中的 482；qwindowkit 实际的 Acrylic 主路径是 Windows 11 system backdrop，不能将其注释当成 Win10 已验证实现。
- Blur / legacy Acrylic 在保留原生非客户区时仍观察到边缘内框 / 色带，尚未达到用户的视觉预期。不能仅凭 effectiveMaterial 或原生 API 返回成功声称修复。
- 后续桌面捕获返回全黑且输入桌面访问被拒绝，未获得可用的 qwindowkit 同背景对照图；Windows 11、macOS 也没有新的视觉验证证据。

### 材质验收与 Acrylic 回退策略（2026-11-18）

- 用户确认 Widgets / QML 的透明与 Blur 效果正常；此前 Blur 尚未验收的记录保留为排查历史。
- 按用户明确要求，Windows 10 的 Acrylic 与 Mica / MicaAlt 一样回退默认 Solid，恢复宿主背景，不使用旧 Accent Acrylic。不强制覆盖应用原有背景颜色。
- Acrylic / Mica / MicaAlt 统一要求 build 22621+；系统能力不足时，保留材质请求、报告版本要求，effectiveMaterial 为 Solid。后续切回 Blur / Transparent 正常应用。
- 现代 Acrylic 保留 `DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_TRANSIENTWINDOW`、左侧 frame 扩展和 Quick 首次表面刷新。这是 qwindowkit 的实际 Acrylic 调用路径；视觉效果仍待 Win11 对应系统验收。
- 删除旧 Accent Acrylic 的状态 4、tint / alpha 与兼容版本分支；既有 `WINDOWHELER_ENABLE_LEGACY_ACCENT` 仅控制 Blur。
- 原生 Blur 不提供半径 / sigma / 迭代次数配置；系统 Acrylic 也不承诺可调半径。可调的应用内部内容模糊应独立于窗口材质功能。

### 透明 / Blur 原生白边与上游修复对齐（2026-11-18）

- 在 HWND 绑定时准备深色原生边框，补齐 qwindowkit 的 UxTheme 原生主题支持与菜单缓存刷新。Windows 10 1903+ 使用 `SetPreferredAppMode(AllowDark/PAM_AUTO)`，1809 使用旧版 `AllowDarkModeForApp`；不设置 ForceDark，不修改 Qt 调色板或渲染器。该原生菜单主题能力是进程级设置，单窗口解绑不关闭它，避免影响其他受管窗口。DWM 明暗属性仍保存、按归属恢复。
- Solid / Transparent / Blur 使用深色原生边框；系统 backdrop 保持按宿主背景选择明暗。移除试验中的 Win11 显式边框颜色和首次材质延迟刷新，未将它们当作上游修复保留。
- Win10 普通材质包含 Blur 均使用 top 1px frame 扩展。Qt Widgets 顶边在 Expose / UpdateRequest 完成后绘制；主题、系统颜色、DWM 强调色变化重新应用外观。框架变更采用 qwindowkit 的 `SWP_NOCOPYBITS` 处理。
- 两项上游 workaround 不直接移植：失焦时扩展整个标题栏会在当前预留 alpha 的宿主上露出深色标题带；WM_SHOWWINDOW 中增减 1px 会导致当前 Quick 窗口隐藏/显示后几何变化。保留普通 top 1px 和已有 Quick 首次材质表面刷新，已撤销这两项回归。
- Win10 build 19045 的受控捕获中，Widgets example 首次透明 / Blur 切换没有纯白边框；两端 Blur 失焦后没有顶部白线。自动下拉框探针仍显示 popup 状态，不能替代真实点击关闭下拉框的人工验收。用户实际首次切换场景仍须复查。Win11 / macOS 视觉效果未在本轮验证。
- Qt 6.8 的 `WM_NCCALCSIZE` 在 `QAbstractNativeEventFilter` 中完成客户区修正，子类化窗口过程直接转发该消息给 Qt，避免在 Qt 已缓存普通标题栏边距后再覆盖矩形。仍用 `SetWindowSubclass` 维护每个窗口的过程链，原生过滤器只处理自身 HWND 的尺寸消息。
- 上游 issue、PR 中文摘要、移植范围和平台验证边界见 [upstream-fixes.md](upstream-fixes.md)。
