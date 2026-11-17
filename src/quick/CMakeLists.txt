qt_add_library(fk_quick ${FRAMELESSKIT_LIBRARY_TYPE})
framelesskit_configure_target(fk_quick Quick)
target_link_libraries(fk_quick PUBLIC FramelessKit::Core Qt6::Qml Qt6::Quick)
target_include_directories(fk_quick PRIVATE "${PROJECT_SOURCE_DIR}/include/FramelessKit")
# The target, rather than a late-loaded QML plugin, wires Quick initialization.
target_sources(fk_quick INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/cmake/QuickBootstrap.cpp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit/QuickBootstrap.cpp>
)
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
set_target_properties(fk_quickplugin PROPERTIES EXPORT_NAME QuickPlugin)
# qmldir is configuration-independent; keep its plugin beside it for multi-config builds.
foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${config}" upper_config)
    set_target_properties(fk_quickplugin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${upper_config} "${QT_QML_OUTPUT_DIRECTORY}/FramelessKit"
        LIBRARY_OUTPUT_DIRECTORY_${upper_config} "${QT_QML_OUTPUT_DIRECTORY}/FramelessKit"
    )
endforeach()
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
install(FILES "${PROJECT_SOURCE_DIR}/cmake/QuickBootstrap.cpp"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit"
)
install(FILES "${PROJECT_SOURCE_DIR}/include/FramelessKit/WindowAgent.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FramelessKit"
)
