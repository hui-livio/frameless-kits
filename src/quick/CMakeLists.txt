# A QML-only consumer may not reference any C++ symbols in the backing library.
# Link its bootstrap object directly so the linker retains the module.
function(windowheler_link_quick_bootstrap target component source)
    set(bootstrap ${target}_bootstrap)
    add_library(${bootstrap} OBJECT ${source})
    add_library(WindowHeler::${component}Bootstrap ALIAS ${bootstrap})
    set_target_properties(${bootstrap} PROPERTIES
        EXPORT_NAME ${component}Bootstrap
        POSITION_INDEPENDENT_CODE ON
        AUTOMOC OFF
        AUTOUIC OFF
    )
    target_include_directories(${bootstrap} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(${bootstrap} PRIVATE Qt6::Core)
    if(NOT WINDOWHELER_BUILD_SHARED_LIBS)
        target_compile_definitions(${bootstrap} PRIVATE WINDOWHELER_STATIC)
    endif()
    target_link_libraries(${target} INTERFACE
        WindowHeler::${component}Bootstrap
        $<TARGET_OBJECTS:WindowHeler::${component}Bootstrap>
    )
    if(WINDOWHELER_INSTALL)
        install(TARGETS ${bootstrap}
            EXPORT WindowHelerTargets
            OBJECTS DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WindowHeler/objects"
        )
    endif()
endfunction()

if(WINDOWHELER_BUILD_FRAMELESS)
qt_add_library(windowheler_frameless_quick ${WINDOWHELER_LIBRARY_TYPE})
windowheler_configure_target(windowheler_frameless_quick FramelessQuick)
set_target_properties(windowheler_frameless_quick PROPERTIES OUTPUT_NAME framelessquick)
windowheler_link_runtime(windowheler_frameless_quick)
target_link_libraries(windowheler_frameless_quick PUBLIC Qt6::Qml Qt6::Quick)
target_include_directories(windowheler_frameless_quick PRIVATE "${PROJECT_SOURCE_DIR}/include/WindowHeler")

qt_add_qml_module(windowheler_frameless_quick
    URI WindowHeler.Frameless
    VERSION 1.0
    NO_PLUGIN
    OUTPUT_TARGETS qml_frameless_output_targets
    SOURCES
        "${PROJECT_SOURCE_DIR}/include/WindowHeler/FramelessAgent.h"
        FramelessAgent.cpp
        QuickInitialization.cpp
)
windowheler_link_quick_bootstrap(windowheler_frameless_quick FramelessQuick QuickBootstrap.cpp)
endif()

if(WINDOWHELER_BUILD_MATERIAL)
qt_add_library(windowheler_material_quick ${WINDOWHELER_LIBRARY_TYPE})
windowheler_configure_target(windowheler_material_quick MaterialQuick)
set_target_properties(windowheler_material_quick PROPERTIES OUTPUT_NAME framelessmaterialquick)
windowheler_link_runtime(windowheler_material_quick)
target_link_libraries(windowheler_material_quick PUBLIC Qt6::Qml Qt6::Quick)
target_include_directories(windowheler_material_quick PRIVATE "${PROJECT_SOURCE_DIR}/include/WindowHeler")
qt_add_qml_module(windowheler_material_quick
    URI WindowHeler.Material
    VERSION 1.0
    NO_PLUGIN
    OUTPUT_TARGETS qml_material_output_targets
    SOURCES
        "${PROJECT_SOURCE_DIR}/include/WindowHeler/MaterialAgent.h"
        MaterialAgent.cpp
)
windowheler_link_quick_bootstrap(windowheler_material_quick MaterialQuick MaterialBootstrap.cpp)
endif()

if(WINDOWHELER_INSTALL)
    install(TARGETS ${qml_frameless_output_targets} ${qml_material_output_targets}
        EXPORT WindowHelerTargets
        RUNTIME DESTINATION "${WINDOWHELER_QML_INSTALL_DIR}"
        LIBRARY DESTINATION "${WINDOWHELER_QML_INSTALL_DIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
    install(FILES
        "${PROJECT_SOURCE_DIR}/include/WindowHeler/FramelessAgent.h"
        "${PROJECT_SOURCE_DIR}/include/WindowHeler/MaterialAgent.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/WindowHeler"
    )
endif()
