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
    PlatformBackend_p.h
    QtFallbackBackend.cpp
    "${PROJECT_SOURCE_DIR}/include/FramelessKit/Material.h"
)
target_link_libraries(fk_core PUBLIC Qt6::Core Qt6::Gui)
if(WIN32)
    target_sources(fk_core PRIVATE WindowsBackend.cpp)
    target_compile_definitions(fk_core PRIVATE
        FRAMELESSKIT_ENABLE_LEGACY_ACCENT=$<BOOL:${ENABLE_LEGACY_ACCENT}>
    )
endif()
install(FILES
    "${PROJECT_SOURCE_DIR}/include/FramelessKit/Initialization.h"
    "${PROJECT_SOURCE_DIR}/include/FramelessKit/Material.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FramelessKit"
)
