# qwindowkit 关键问题复核与移植记录

复核日期：2026-11-18。支持范围固定为 Qt 6.8+，不引入 Qt 5 或早期 Qt 6 的兼容分支。参考 GitHub issue / PR 原文，以及本地 `D:/app/qwindowkit` 的提交代码（复核时 HEAD 为 `2072958`）。这里只整理与本工程有关的记录，不代表遍历全部历史问题。

## 中文摘要与处理结果

| 上游记录 | 问题与修复要点 | 本工程处理 |
| --- | --- | --- |
| [Issue #168](https://github.com/stdware/qwindowkit/issues/168) | 保留系统边框的置顶 QML 窗口切换焦点时，顶部短暂出现白条。维护者指出 Qt 5 问题；讨论中的延迟缩放只是用户 workaround，没有通用已合并修复。 | 不将其当作当前 Win10 首次透明白框的同一根因。对照上游实际边框初始化、菜单主题、绘制时机修正；见下文。 |
| [Issue #246](https://github.com/stdware/qwindowkit/issues/246) | 弹出阻塞窗口后顶部灰线消失；原文信息不足，维护者要求补充描述。 | 没有可移植补丁。修复本工程 `paintNativeFrame()` 未接入的问题，让 Widgets 在 Qt 栅格提交后绘制顶边；不宣称解决了上游全部场景。 |
| [PR #252](https://github.com/stdware/qwindowkit/pull/252) | Quick Agent 比窗口先销毁时，遗留的边框 Item 继续引用已释放 Context，导致崩溃。上游让 Agent 拥有并先销毁 Item，用 QPointer 保护窗口先销毁的顺序。 | 本工程没有该 BorderItem，已有 Controller 所有权和带 context 的定时回调；无需引入边框 Item。已检查并修复同类输入回调生命周期漏洞。 |
| [提交 6d353bb](https://github.com/stdware/qwindowkit/commit/6d353bb2cf899c463d0ae8844cc58b8309271549) | 原生菜单、WinId 和窗口消息存在嵌套回调：Agent 或 HWND 可能在回调内销毁、替换。仅检查 Context 指针不足，需要窗口身份与代数检查。 | 系统菜单返回时验证 HWND / generation；补齐悬停、捕获释放、尺寸计算、解绑与重绑后的存活检查。旧解绑不能覆盖重入建立的新绑定；Agent 已销毁但原窗口仍存活时继续让 Qt 处理原生消息。 |
| [Issue #212](https://github.com/stdware/qwindowkit/issues/212)、[提交 bfcd970](https://github.com/stdware/qwindowkit/commit/bfcd9701888daeca93d53fe7e54402cc9966bf6a) | Linux 缩放后光标不恢复。上游后续同时处理逐轴尺寸约束、状态变更、完整光标恢复与光标所属窗口。 | 回退后端观察约束和窗口状态；按下/释放时重新判断缩放边缘；保存并恢复完整 QCursor；尊重 `MSWindowsFixedSizeDialogHint`。关闭窗口或光标变更回调销毁对象时停止继续访问。 |
| [PR #199](https://github.com/stdware/qwindowkit/pull/199) | 隐藏或禁用系统按钮后仍响应原生命中；最终补丁检查按钮自身的 visible / enabled。 | 已有覆盖：HostAdapter 检查可见性、所属窗口及裁剪；HitTestModel 检查 enabled。禁用但可见的交互控件继续保留客户区，避免误触标题栏拖动。 |
| [Issue #232](https://github.com/stdware/qwindowkit/issues/232) | Qt 6.8.3 的 QDialog 最大化异常，置顶切换时窗口下移。讨论未给出确定的通用修复。 | 核对原生矩形后发现现有 WM_NCCALCSIZE 在 Qt 之后修正客户区，造成 Qt 缓存普通标题栏边距并报告几何警告。已对齐 qwindowkit 的原生事件过滤阶段处理，让 Qt 记录最终边距；本轮 QDialog 状态、Qt/Win32 客户区及还原检查通过，但不宣称覆盖该 issue 的全部条件。另撤回破坏 Quick hide/show 几何的首次显示尺寸刷新。 |
| [Issue #166](https://github.com/stdware/qwindowkit/issues/166)、[提交 60cd5ce](https://github.com/stdware/qwindowkit/commit/60cd5ce) | macOS Qt 6.8 Widgets 自动安全区边距使自定义标题栏整体下移。上游关闭宿主、标题栏子树与中间容器的自动安全区边距。 | 增加 Cocoa 专用 Widgets 私有处理器，跟踪属性原值，在标题栏替换、重设父对象及解绑时恢复。Qt 6.8.3 SDK 已存在 `WA_ContentsMarginsRespectsSafeArea`。Mac 原生观感待实机验证。 |
| [Issue #118](https://github.com/stdware/qwindowkit/issues/118) | Windows 全屏窗口改变 DPI 的历史问题；维护者指出 Qt 6 已处理相关消息行为。 | 当前最低 Qt 6.8，保留 Qt DPI 消息处理，不添加旧 Qt workaround。跨物理屏幕 DPI 仍待实机验证。 |
| [PR #203](https://github.com/stdware/qwindowkit/pull/203) | 添加 X11 `_GTK_SHOW_WINDOW_MENU`、Wayland `xdg_toplevel` 系统菜单，依赖桌面环境和输入 serial。讨论中特别指出 X11 坐标受高 DPI 影响，Wayland 需要保留输入事件 serial。 | 已接入：X11 使用实际 root 坐标、只释放指针抓取于请求前、检查真实 root 的 `_NET_SUPPORTED`；Wayland 同步捕获 serial/seat/surface，使用 Qt 高 DPI 转换成 surface-local 坐标，拒绝过期或被重入替换的输入上下文。两端保留 Qt 公共移动/缩放回退。 |

## Windows 白边实现取舍

- 在 HWND 绑定时启用深色原生边框，对齐 `Windows10BorderHandler::setupNecessaryAttributes()`，避免等到首次下拉框切换材质时才初始化。
- 对齐 `dark-mode` 的 UxTheme 调用：支持原生系统明暗主题，设置窗口 DWM 明暗属性，刷新原生菜单主题缓存。这里包含进程级主题能力设置，不强制修改 Qt 的控件调色板；单个 Agent 解绑不会关闭整个进程的原生主题支持。
- Solid、Transparent、Blur 共用深色边框；Mica / MicaAlt / Acrylic 保持宿主明暗选择。解绑仅恢复仍由模块持有的窗口属性。
- Widgets 在 Expose / UpdateRequest 的 Qt 绘制结束后补顶边，Win10 Blur 保留 top 1px frame 扩展。统一隐藏原生 caption 按钮，Alt+Space 覆盖所有材质。
- 不移植导致本工程回归的两项 workaround：失焦时扩展整个标题栏会露出暗色标题带；在 WM_SHOWWINDOW 中直接增减尺寸会使 Quick hide/show 几何漂移。保留已有的 Quick 首次材质合成表面刷新。
- 保留 `WS_THICKFRAME` 和 DWM 非客户区绘制，边框、阴影和缩放仍由原生窗口参与处理。

## 验证边界

- 未新增 UT 或正式测试构建目标。临时交互/生命周期探针仅位于忽略的 `build/visual-probe*` 目录，调用公开 Agent API。
- Windows 10 build 19045、Qt 6.8.3：原生窗口探针通过悬停回调销毁 Agent、Quick Agent 先销毁、Alt+Space 返回、QDialog 最大化/还原、两端透明、Qt/Win32 客户区一致及 Quick hide/show 几何检查。
- 同一 Qt SDK 的 offscreen 回退探针通过角落缩放光标、约束变更恢复、单轴缩放、最大化恢复和固定尺寸提示检查。这是共用后端验证，不等同于 Linux 窗口管理器实测。
- 首次材质受控捕获中未见纯白边框，窗口仍激活；自动下拉框探针的 popup 状态仍在，真实点击关闭下拉框的路径需要用户在 example 复查。
- 本轮未验证 Win11 系统材质、跨屏 DPI 和 macOS 原生运行效果。未将上游 issue 的标签或关闭状态作为本工程已通过的证据。

## Linux X11 / Wayland 适配边界

- X11 只在 Qt 构建提供 `QX11Application` 且 XCB 开发头可用时编译。`libxcb.so.1` 在运行时加载；不增加 Xlib、XCB 或 xcb QPA 插件的直接链接依赖。发送之前检查当前 root 的 `_NET_SUPPORTED` 是否包含 `_GTK_SHOW_WINDOW_MENU`，并验证协议响应、真实原生窗口、坐标和 flush。收到服务器确认仅表示请求被接受，不表示菜单实际显示。
- Wayland 只在同一个 Qt 安装提供 `Qt6::GuiPrivate`、Wayland 原生接口和 client 头时编译。安装 target 和 consumer 按构建时 Qt 精确版本匹配；消费者不需要安装 private headers。运行时通过 Qt Wayland native interface 取得当前 spontaneous event 的 seat/serial 和 xdg_toplevel。嵌套输入、表面替换或 serial 改变时拒绝旧请求；menu request 仅在当前输入派发栈同步发送。协议未公开可查询的当前 menu capability，compositor 可合法静默忽略请求。
- X11 或 Wayland 适配可分别用 `WINDOWHELER_ENABLE_X11=OFF` / `WINDOWHELER_ENABLE_WAYLAND=OFF` 禁用；依赖/接口不可用时配置保持成功并回到 Qt move/resize。
- WSL Arch / Qt 6.12.0：Release shared build、static build、protocols-disabled build 均通过；Widgets / Quick 在 X11 与 Wayland 的启动和材质循环通过。安装后的 shared / static 外部消费者也通过。WSLg Wayland v1 / Weston 未提供可见系统菜单，XWayland `_NET_SUPPORTED` 未提供 GTK 菜单 atom；所以探针分别确认明确诊断或当前输入 serial 缺失，不声称观察到了菜单 UI。
- 忽略的 `build/linux-menu-probe-src` 使用公开 Agents 跑 30 个 X11 / Wayland 输入、交互区、surface 生命周期和销毁/重绑场景，两个进程均以 0 退出。没有新增 UT/CTest 目标或链接外部平台库。
