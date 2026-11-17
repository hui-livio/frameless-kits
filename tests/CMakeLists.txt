
function(framelesskit_add_test target source)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE FramelessKit::Core Qt6::Test)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/core")
    add_test(NAME ${target} COMMAND ${target})
endfunction()

framelesskit_add_test(framelesskit_core_tests CoreTests.cpp)
framelesskit_add_test(framelesskit_runtime_tests RuntimeTests.cpp)

if(BUILD_WIDGETS)
    framelesskit_add_test(framelesskit_widgets_tests WidgetsTests.cpp)
    target_link_libraries(framelesskit_widgets_tests PRIVATE FramelessKit::Widgets)
endif()

if(BUILD_QUICK)
    framelesskit_add_test(framelesskit_quick_tests QuickTests.cpp)
    target_link_libraries(framelesskit_quick_tests PRIVATE FramelessKit::Quick)
endif()

if(BUILD_EXAMPLES AND BUILD_WIDGETS)
    add_test(NAME framelesskit_widgets_demo_smoke COMMAND framelesskit_widgets_demo --smoke-test)
endif()

if(BUILD_EXAMPLES AND BUILD_QUICK)
    add_test(NAME framelesskit_qml_demo_smoke COMMAND framelesskit_quick_demo --smoke-test)
endif()

if(BUILD_WIDGETS OR BUILD_QUICK)
    add_test(NAME framelesskit_installed_consumer
        COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
            "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DCONFIG=$<CONFIG>"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DTOOLSET=${CMAKE_GENERATOR_TOOLSET}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DQT6_DIR=${Qt6_DIR}"
            "-DQT_BIN_DIR=$<TARGET_FILE_DIR:Qt6::Core>"
            "-DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
            "-DWIDGETS=${BUILD_WIDGETS}"
            "-DQUICK=${BUILD_QUICK}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/RunInstalledConsumer.cmake"
    )
endif()

get_property(test_names DIRECTORY PROPERTY TESTS)

set_tests_properties(${test_names} PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
    TIMEOUT 30
)

if(TEST framelesskit_installed_consumer)
    set_tests_properties(framelesskit_installed_consumer PROPERTIES TIMEOUT 180)
endif()
