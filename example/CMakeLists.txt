if(WINDOWHELER_BUILD_WIDGETS)
    qt_add_executable(windowheler_widgets_demo WidgetsDemo.cpp DemoOptions.h)
    target_link_libraries(windowheler_widgets_demo PRIVATE WindowHeler::FramelessWidgets
        WindowHeler::MaterialWidgets)
    if(MSVC)
        target_compile_options(windowheler_widgets_demo PRIVATE /utf-8)
    endif()
endif()

if(WINDOWHELER_BUILD_QUICK)
    qt_add_executable(windowheler_quick_demo QuickMain.cpp DemoOptions.h)
    target_link_libraries(windowheler_quick_demo PRIVATE WindowHeler::FramelessQuick
        WindowHeler::MaterialQuick)
    target_compile_definitions(windowheler_quick_demo PRIVATE
        WINDOWHELER_DEMO_IMPORT_PATH="${QT_QML_OUTPUT_DIRECTORY}"
    )
    qt_add_qml_module(windowheler_quick_demo
        URI FramelessDemo
        VERSION 1.0
        QML_FILES Main.qml
        IMPORTS TARGET windowheler_frameless_quick
    )
endif()
