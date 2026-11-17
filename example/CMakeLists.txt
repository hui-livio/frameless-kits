if(FRAMELESSKIT_BUILD_WIDGETS)
    qt_add_executable(framelesskit_widgets_demo WidgetsDemo.cpp DemoOptions.h)
    target_link_libraries(framelesskit_widgets_demo PRIVATE FramelessKit::Widgets)
    if(MSVC)
        target_compile_options(framelesskit_widgets_demo PRIVATE /utf-8)
    endif()
endif()

if(FRAMELESSKIT_BUILD_QUICK)
    qt_add_executable(framelesskit_quick_demo QuickMain.cpp DemoOptions.h)
    target_link_libraries(framelesskit_quick_demo PRIVATE FramelessKit::Quick)
    if(NOT FRAMELESSKIT_BUILD_SHARED_LIBS)
        target_link_libraries(framelesskit_quick_demo PRIVATE FramelessKit::QuickPlugin)
    endif()
    target_compile_definitions(framelesskit_quick_demo PRIVATE
        FRAMELESSKIT_DEMO_IMPORT_PATH="${QT_QML_OUTPUT_DIRECTORY}"
    )
    qt_add_qml_module(framelesskit_quick_demo
        URI FramelessDemo
        VERSION 1.0
        QML_FILES Main.qml
        IMPORTS TARGET fk_quick
    )
endif()
