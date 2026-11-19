qt_add_library(fk_core OBJECT)
set_target_properties(fk_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
framelesskit_configure_target(fk_core Core)
target_sources(fk_core PRIVATE
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
    "${PROJECT_SOURCE_DIR}/include/FramelessKit/Material.h"
)
target_link_libraries(fk_core PUBLIC Qt6::Core Qt6::Gui)
if(WIN32)
    target_sources(fk_core PRIVATE WindowsBackend.cpp WindowsMaterial.cpp WindowsMaterial_p.h)
    target_link_libraries(fk_core PRIVATE comctl32 user32 shell32 dwmapi gdi32)
    target_compile_definitions(fk_core PRIVATE
        FRAMELESSKIT_ENABLE_LEGACY_ACCENT=$<BOOL:${FRAMELESSKIT_ENABLE_LEGACY_ACCENT}>
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(fk_core PRIVATE CocoaBackend.mm)
    set_source_files_properties(CocoaBackend.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(fk_core PRIVATE
        "-framework AppKit"
        "-framework QuartzCore"
        "-framework CoreGraphics"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(fk_core PRIVATE LinuxBackend.cpp LinuxSystemMenu_p.h)
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    set(framelesskit_has_x11 OFF)
    set(framelesskit_has_wayland OFF)
    if(FRAMELESSKIT_ENABLE_X11)
        find_path(FRAMELESSKIT_XCB_INCLUDE_DIR NAMES xcb/xcb.h)
        if(FRAMELESSKIT_XCB_INCLUDE_DIR)
            cmake_push_check_state(RESET)
            set(CMAKE_REQUIRED_LIBRARIES Qt6::Gui)
            set(CMAKE_REQUIRED_INCLUDES "${FRAMELESSKIT_XCB_INCLUDE_DIR}")
            check_cxx_source_compiles("
                #include <QGuiApplication>
                #include <QtGui/qguiapplication_platform.h>
                #include <xcb/xcb.h>
                int main() {
                    using Native = QNativeInterface::QX11Application;
                    Native *native = nullptr;
                    return native ? (native->connection() != nullptr) : 0;
                }" FRAMELESSKIT_CAN_USE_X11)
            cmake_pop_check_state()
            if(FRAMELESSKIT_CAN_USE_X11)
                set(framelesskit_has_x11 ON)
                target_sources(fk_core PRIVATE X11SystemMenu.cpp)
                target_include_directories(fk_core PRIVATE "${FRAMELESSKIT_XCB_INCLUDE_DIR}")
            endif()
        endif()
    endif()
    if(FRAMELESSKIT_ENABLE_WAYLAND)
        find_package(Qt6GuiPrivate ${Qt6_VERSION} EXACT QUIET CONFIG GLOBAL)
        find_path(FRAMELESSKIT_WAYLAND_INCLUDE_DIR NAMES wayland-client-core.h)
        # Qt 6.8 can define private imported targets even when private headers
        # are not installed. Avoid a fatal try_compile import-path error.
        get_target_property(framelesskit_gui_includes Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(framelesskit_core_includes Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
        find_path(framelesskit_gui_private NAMES QtGui/private/qhighdpiscaling_p.h
            PATHS ${framelesskit_gui_includes} PATH_SUFFIXES "${Qt6_VERSION}" NO_DEFAULT_PATH NO_CACHE)
        find_path(framelesskit_core_private NAMES QtCore/private/qglobal_p.h
            PATHS ${framelesskit_core_includes} PATH_SUFFIXES "${Qt6_VERSION}" NO_DEFAULT_PATH NO_CACHE)
        if(TARGET Qt6::GuiPrivate AND TARGET Qt6::CorePrivate AND FRAMELESSKIT_WAYLAND_INCLUDE_DIR
            AND framelesskit_gui_private AND framelesskit_core_private
            AND EXISTS "${framelesskit_gui_private}/QtGui/qpa/qplatformnativeinterface.h")
            cmake_push_check_state(RESET)
            set(CMAKE_REQUIRED_LIBRARIES Qt6::GuiPrivate)
            set(CMAKE_REQUIRED_INCLUDES "${FRAMELESSKIT_WAYLAND_INCLUDE_DIR}")
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
                }" FRAMELESSKIT_CAN_USE_WAYLAND)
            cmake_pop_check_state()
            if(FRAMELESSKIT_CAN_USE_WAYLAND)
                set(framelesskit_has_wayland ON)
                set(FRAMELESSKIT_PRIVATE_QT_VERSION "${Qt6_VERSION}" PARENT_SCOPE)
                target_sources(fk_core PRIVATE WaylandSystemMenu.cpp)
                target_include_directories(fk_core PRIVATE "${FRAMELESSKIT_WAYLAND_INCLUDE_DIR}")
                # QPA/high-DPI adaptation stays private to this translation unit.
                # Runtime symbols are supplied by the public Qt6::Gui dependency.
                target_link_libraries(fk_core PRIVATE $<BUILD_INTERFACE:Qt6::GuiPrivate>)
            endif()
        endif()
    endif()
    target_compile_definitions(fk_core PRIVATE
        FRAMELESSKIT_HAS_LINUX_BACKEND=1
        FRAMELESSKIT_HAS_X11=$<BOOL:${framelesskit_has_x11}>
        FRAMELESSKIT_HAS_WAYLAND=$<BOOL:${framelesskit_has_wayland}>
    )
    message(STATUS "FramelessKit Linux native menus: X11=${framelesskit_has_x11}, Wayland=${framelesskit_has_wayland}")
    if(FRAMELESSKIT_ENABLE_X11 AND NOT framelesskit_has_x11)
        message(STATUS "X11 menu unavailable: Qt X11 native interface or XCB headers missing; Qt fallback remains enabled")
    endif()
    if(FRAMELESSKIT_ENABLE_WAYLAND AND NOT framelesskit_has_wayland)
        message(STATUS "Wayland menu unavailable: Qt Wayland native interface, matching GuiPrivate or client headers missing; Qt fallback remains enabled")
    endif()
endif()

if(FRAMELESSKIT_INSTALL)
    install(FILES
        "${PROJECT_SOURCE_DIR}/include/FramelessKit/Export.h"
        "${PROJECT_SOURCE_DIR}/include/FramelessKit/Initialization.h"
        "${PROJECT_SOURCE_DIR}/include/FramelessKit/Material.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FramelessKit"
    )
endif()
