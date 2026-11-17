qt_add_library(fk_quick ${FRAMELESSKIT_LIBRARY_TYPE})
framelesskit_configure_target(fk_quick Quick)
target_link_libraries(fk_quick PUBLIC FramelessKit::Core Qt6::Qml Qt6::Quick)
target_include_directories(fk_quick PRIVATE "${PROJECT_SOURCE_DIR}/include/FramelessKit")
# Link a precompiled bootstrap directly into consumers, bypassing static archive
# elimination without distributing implementation sources in the installed package.
add_library(fk_quick_bootstrap OBJECT QuickBootstrap.cpp)
add_library(FramelessKit::QuickBootstrap ALIAS fk_quick_bootstrap)
set_target_properties(fk_quick_bootstrap PROPERTIES
    EXPORT_NAME QuickBootstrap
    POSITION_INDEPENDENT_CODE ON
    AUTOMOC OFF
    AUTOUIC OFF
)
target_compile_features(fk_quick_bootstrap PRIVATE cxx_std_20)
target_link_libraries(fk_quick_bootstrap PRIVATE FramelessKit::Core)
# Forward the target as well as its objects so selected consumer builds also
# build the bootstrap (not just builds of the default ALL target).
target_link_libraries(fk_quick INTERFACE
    FramelessKit::QuickBootstrap
    $<TARGET_OBJECTS:FramelessKit::QuickBootstrap>
)
if(FRAMELESSKIT_INSTALL)
    install(TARGETS fk_quick_bootstrap
        EXPORT FramelessKitQuickTargets
        OBJECTS DESTINATION "${CMAKE_INSTALL_LIBDIR}/FramelessKit/objects"
    )
endif()
qt_add_qml_module(fk_quick
    URI FramelessKit
    VERSION 1.0
    CLASS_NAME FramelessKitPlugin
    OUTPUT_TARGETS qml_output_targets
    SOURCES
        "${PROJECT_SOURCE_DIR}/include/FramelessKit/WindowAgent.h"
        WindowAgent.cpp
        QuickInitialization.cpp
)
add_library(FramelessKit::QuickPlugin ALIAS fk_quickplugin)
set_target_properties(fk_quickplugin PROPERTIES EXPORT_NAME QuickPlugin AUTOMOC ON)
# qmldir is configuration-independent; keep its plugin beside it for multi-config builds.
foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${config}" upper_config)
    set_target_properties(fk_quickplugin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${upper_config} "${QT_QML_OUTPUT_DIRECTORY}/FramelessKit"
        LIBRARY_OUTPUT_DIRECTORY_${upper_config} "${QT_QML_OUTPUT_DIRECTORY}/FramelessKit"
    )
endforeach()
if(FRAMELESSKIT_INSTALL)
    install(TARGETS fk_quickplugin ${qml_output_targets}
        EXPORT FramelessKitQuickTargets
        RUNTIME DESTINATION "${FRAMELESSKIT_QML_INSTALL_DIR}/FramelessKit"
        LIBRARY DESTINATION "${FRAMELESSKIT_QML_INSTALL_DIR}/FramelessKit"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        OBJECTS DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit/objects"
    )
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/FramelessKit/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/FramelessKit/fk_quick.qmltypes"
        DESTINATION "${FRAMELESSKIT_QML_INSTALL_DIR}/FramelessKit"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/include/FramelessKit/WindowAgent.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FramelessKit"
    )
endif()
