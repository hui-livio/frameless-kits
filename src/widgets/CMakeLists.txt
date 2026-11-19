if(WINDOWHELER_BUILD_FRAMELESS)
qt_add_library(windowheler_frameless_widgets ${WINDOWHELER_LIBRARY_TYPE})
windowheler_configure_target(windowheler_frameless_widgets FramelessWidgets)
set_target_properties(windowheler_frameless_widgets PROPERTIES OUTPUT_NAME framelesswidget)
windowheler_link_runtime(windowheler_frameless_widgets)
target_sources(windowheler_frameless_widgets PRIVATE
    FramelessWidgetAgent.cpp
    "${PROJECT_SOURCE_DIR}/include/WindowHeler/FramelessWidgetAgent.h"
)
if(APPLE)
    target_sources(windowheler_frameless_widgets PRIVATE CocoaWidgetSafeArea_p.h)
endif()
target_link_libraries(windowheler_frameless_widgets PUBLIC Qt6::Widgets)
endif()

if(WINDOWHELER_BUILD_MATERIAL)
qt_add_library(windowheler_material_widgets ${WINDOWHELER_LIBRARY_TYPE})
windowheler_configure_target(windowheler_material_widgets MaterialWidgets)
set_target_properties(windowheler_material_widgets PROPERTIES OUTPUT_NAME framelessmaterialwidget)
windowheler_link_runtime(windowheler_material_widgets)
target_sources(windowheler_material_widgets PRIVATE
    MaterialWidgetAgent.cpp
    "${PROJECT_SOURCE_DIR}/include/WindowHeler/MaterialWidgetAgent.h"
)
target_link_libraries(windowheler_material_widgets PUBLIC Qt6::Widgets)
endif()

if(WINDOWHELER_INSTALL)
    install(FILES
        "${PROJECT_SOURCE_DIR}/include/WindowHeler/FramelessWidgetAgent.h"
        "${PROJECT_SOURCE_DIR}/include/WindowHeler/MaterialWidgetAgent.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/WindowHeler"
    )
endif()
