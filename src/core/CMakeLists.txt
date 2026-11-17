qt_add_library(fk_core ${FRAMELESSKIT_LIBRARY_TYPE})
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
    target_sources(fk_core PRIVATE WindowsBackend.cpp)
    target_link_libraries(fk_core PRIVATE comctl32 user32 shell32 dwmapi gdi32)
    target_compile_definitions(fk_core PRIVATE
        FRAMELESSKIT_ENABLE_LEGACY_ACCENT=$<BOOL:${FRAMELESSKIT_ENABLE_LEGACY_ACCENT}>
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(fk_core PRIVATE CocoaBackend.mm)
    set_source_files_properties(CocoaBackend.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(fk_core PRIVATE "-framework AppKit")
endif()

if(FRAMELESSKIT_INSTALL)
    install(FILES
        "${PROJECT_SOURCE_DIR}/include/FramelessKit/Export.h"
        "${PROJECT_SOURCE_DIR}/include/FramelessKit/Initialization.h"
        "${PROJECT_SOURCE_DIR}/include/FramelessKit/Material.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FramelessKit"
    )
endif()
