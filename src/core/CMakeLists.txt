qt_add_library(windowheler_runtime SHARED)
set_target_properties(windowheler_runtime PROPERTIES OUTPUT_NAME windowhelerruntime)
set_target_properties(windowheler_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
windowheler_configure_target(windowheler_runtime Runtime)
target_sources(windowheler_runtime PRIVATE
    Initialization.cpp
    Runtime_p.h
    WindowController.cpp
    WindowController.h
    HostAdapter_p.h
    HitTestModel_p.h
    MaterialController_p.h
    FrameGeometry_p.h
    PlatformBackend.cpp
    PlatformBackend_p.h
    QtFallbackBackend.cpp
    "${PROJECT_SOURCE_DIR}/include/WindowHeler/Material.h"
)
target_link_libraries(windowheler_runtime PUBLIC Qt6::Core Qt6::Gui)
if(WIN32)
    target_sources(windowheler_runtime PRIVATE WindowsBackend.cpp WindowsMaterial.cpp WindowsMaterial_p.h)
    target_link_libraries(windowheler_runtime PRIVATE comctl32 user32 shell32 dwmapi gdi32)
    target_compile_definitions(windowheler_runtime PRIVATE
        WINDOWHELER_ENABLE_LEGACY_ACCENT=$<BOOL:${WINDOWHELER_ENABLE_LEGACY_ACCENT}>
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(windowheler_runtime PRIVATE CocoaBackend.mm)
    set_source_files_properties(CocoaBackend.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(windowheler_runtime PRIVATE
        "-framework AppKit"
        "-framework QuartzCore"
        "-framework CoreGraphics"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(windowheler_runtime PRIVATE LinuxBackend.cpp LinuxSystemMenu_p.h)
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    set(windowheler_has_x11 OFF)
    set(windowheler_has_wayland OFF)
    if(WINDOWHELER_ENABLE_X11)
        find_path(WINDOWHELER_XCB_INCLUDE_DIR NAMES xcb/xcb.h)
        if(WINDOWHELER_XCB_INCLUDE_DIR)
            cmake_push_check_state(RESET)
            set(CMAKE_REQUIRED_LIBRARIES Qt6::Gui)
            set(CMAKE_REQUIRED_INCLUDES "${WINDOWHELER_XCB_INCLUDE_DIR}")
            check_cxx_source_compiles("
                #include <QGuiApplication>
                #include <QtGui/qguiapplication_platform.h>
                #include <xcb/xcb.h>
                int main() {
                    using Native = QNativeInterface::QX11Application;
                    Native *native = nullptr;
                    return native ? (native->connection() != nullptr) : 0;
                }" WINDOWHELER_CAN_USE_X11)
            cmake_pop_check_state()
            if(WINDOWHELER_CAN_USE_X11)
                set(windowheler_has_x11 ON)
                target_sources(windowheler_runtime PRIVATE X11SystemMenu.cpp)
                target_include_directories(windowheler_runtime PRIVATE "${WINDOWHELER_XCB_INCLUDE_DIR}")
            endif()
        endif()
    endif()
    if(WINDOWHELER_ENABLE_WAYLAND)
        find_package(Qt6GuiPrivate ${Qt6_VERSION} EXACT QUIET CONFIG GLOBAL)
        find_path(WINDOWHELER_WAYLAND_INCLUDE_DIR NAMES wayland-client-core.h)
        # Qt 6.8 can define private imported targets even when private headers
        # are not installed. Avoid a fatal try_compile import-path error.
        get_target_property(windowheler_gui_includes Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(windowheler_core_includes Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
        find_path(windowheler_gui_private NAMES QtGui/private/qhighdpiscaling_p.h
            PATHS ${windowheler_gui_includes} PATH_SUFFIXES "${Qt6_VERSION}" NO_DEFAULT_PATH NO_CACHE)
        find_path(windowheler_core_private NAMES QtCore/private/qglobal_p.h
            PATHS ${windowheler_core_includes} PATH_SUFFIXES "${Qt6_VERSION}" NO_DEFAULT_PATH NO_CACHE)
        if(TARGET Qt6::GuiPrivate AND TARGET Qt6::CorePrivate AND WINDOWHELER_WAYLAND_INCLUDE_DIR
            AND windowheler_gui_private AND windowheler_core_private
            AND EXISTS "${windowheler_gui_private}/QtGui/qpa/qplatformnativeinterface.h")
            cmake_push_check_state(RESET)
            set(CMAKE_REQUIRED_LIBRARIES Qt6::GuiPrivate)
            set(CMAKE_REQUIRED_INCLUDES "${WINDOWHELER_WAYLAND_INCLUDE_DIR}")
            check_cxx_source_compiles("
                #include <QGuiApplication>
                #include <QtGui/qguiapplication_platform.h>
                #include <QtGui/qpa/qplatformnativeinterface.h>
                #include <QtGui/private/qhighdpiscaling_p.h>
                #include <wayland-client-core.h>
                int main() {
                    using Marshal = decltype(&wl_proxy_marshal_flags);
                    Marshal marshal = nullptr;
                    auto *native = qGuiApp
                        ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
                    auto *platform = QGuiApplication::platformNativeInterface();
                    QWindow *window = nullptr;
                    const auto point = QHighDpi::toNativeLocalPosition(QPointF(1, 1), window);
                    return native && platform && native->lastInputSeat()
                        ? int(native->lastInputSerial()) : int(point.x()) + (marshal != nullptr);
                }" WINDOWHELER_CAN_USE_WAYLAND)
            cmake_pop_check_state()
            if(WINDOWHELER_CAN_USE_WAYLAND)
                set(windowheler_has_wayland ON)
                set(WINDOWHELER_PRIVATE_QT_VERSION "${Qt6_VERSION}" PARENT_SCOPE)
                target_sources(windowheler_runtime PRIVATE WaylandSystemMenu.cpp)
                target_include_directories(windowheler_runtime PRIVATE "${WINDOWHELER_WAYLAND_INCLUDE_DIR}")
                # QPA/high-DPI adaptation stays private to this translation unit.
                # Runtime symbols are supplied by the public Qt6::Gui dependency.
                target_link_libraries(windowheler_runtime PRIVATE $<BUILD_INTERFACE:Qt6::GuiPrivate>)
            endif()
        endif()
    endif()
    target_compile_definitions(windowheler_runtime PRIVATE
        WINDOWHELER_HAS_LINUX_BACKEND=1
        WINDOWHELER_HAS_X11=$<BOOL:${windowheler_has_x11}>
        WINDOWHELER_HAS_WAYLAND=$<BOOL:${windowheler_has_wayland}>
    )
    message(STATUS "WindowHeler Linux native menus: X11=${windowheler_has_x11}, Wayland=${windowheler_has_wayland}")
    if(WINDOWHELER_ENABLE_X11 AND NOT windowheler_has_x11)
        message(STATUS "X11 menu unavailable: Qt X11 native interface or XCB headers missing; Qt fallback remains enabled")
    endif()
    if(WINDOWHELER_ENABLE_WAYLAND AND NOT windowheler_has_wayland)
        message(STATUS "Wayland menu unavailable: Qt Wayland native interface, matching GuiPrivate or client headers missing; Qt fallback remains enabled")
    endif()
endif()

if(WINDOWHELER_INSTALL)
    install(FILES
        "${PROJECT_SOURCE_DIR}/include/WindowHeler/Export.h"
        "${PROJECT_SOURCE_DIR}/include/WindowHeler/Initialization.h"
        "${PROJECT_SOURCE_DIR}/include/WindowHeler/Material.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/WindowHeler"
    )
endif()
