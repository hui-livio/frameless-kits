
function(framelesskit_add_test target source)
    qt_add_executable(${target} ${source})
    set_target_properties(${target} PROPERTIES AUTOMOC ON)
    target_link_libraries(${target} PRIVATE FramelessKit::Core Qt6::Test)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/core")
    add_test(NAME ${target} COMMAND ${target})
endfunction()

framelesskit_add_test(framelesskit_core_tests CoreTests.cpp)
framelesskit_add_test(framelesskit_runtime_tests RuntimeTests.cpp)

if(FRAMELESSKIT_BUILD_WIDGETS)
    framelesskit_add_test(framelesskit_widgets_tests WidgetsTests.cpp)
    target_link_libraries(framelesskit_widgets_tests PRIVATE FramelessKit::Widgets)
endif()

if(FRAMELESSKIT_BUILD_QUICK)
    framelesskit_add_test(framelesskit_quick_tests QuickTests.cpp)
    target_link_libraries(framelesskit_quick_tests PRIVATE FramelessKit::Quick)
endif()

if(FRAMELESSKIT_BUILD_EXAMPLES AND FRAMELESSKIT_BUILD_WIDGETS)
    add_test(NAME framelesskit_widgets_demo_smoke COMMAND framelesskit_widgets_demo --smoke-test)
endif()

if(FRAMELESSKIT_BUILD_EXAMPLES AND FRAMELESSKIT_BUILD_QUICK)
    add_test(NAME framelesskit_qml_demo_smoke COMMAND framelesskit_quick_demo --smoke-test)
endif()

if(PROJECT_IS_TOP_LEVEL AND FRAMELESSKIT_INSTALL AND (FRAMELESSKIT_BUILD_WIDGETS OR FRAMELESSKIT_BUILD_QUICK))
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
            "-DWIDGETS=${FRAMELESSKIT_BUILD_WIDGETS}"
            "-DQUICK=${FRAMELESSKIT_BUILD_QUICK}"
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

# Compile the source header without any FramelessKit target or generated includes.
foreach(mode default static core widgets quick)
    set(check_target framelesskit_export_${mode}_check)
    add_library(${check_target} OBJECT ExportHeaderChecks.cpp)
    set_target_properties(${check_target} PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    target_compile_features(${check_target} PRIVATE cxx_std_20)
    target_include_directories(${check_target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(${check_target} PRIVATE Qt6::Core)
    string(TOUPPER "${mode}" upper_mode)
    if(mode STREQUAL "static")
        target_compile_definitions(${check_target} PRIVATE FRAMELESSKIT_STATIC)
    elseif(NOT mode STREQUAL "default")
        target_compile_definitions(${check_target} PRIVATE FRAMELESSKIT_BUILD_${upper_mode})
    endif()
endforeach()

if(WIN32 AND FRAMELESSKIT_BUILD_WIDGETS)
    framelesskit_add_test(framelesskit_widgets_native_tests WidgetsNativeTests.cpp)
    target_link_libraries(framelesskit_widgets_native_tests PRIVATE FramelessKit::Widgets user32)
    set_tests_properties(framelesskit_widgets_native_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=windows;QT_FORCE_STDERR_LOGGING=1"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        TIMEOUT 90
        RUN_SERIAL TRUE
    )
endif()

if(WIN32 AND FRAMELESSKIT_BUILD_QUICK)
    framelesskit_add_test(framelesskit_quick_native_tests QuickNativeTests.cpp)
    target_link_libraries(framelesskit_quick_native_tests PRIVATE FramelessKit::Quick user32)
    set_tests_properties(framelesskit_quick_native_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=windows;QT_FORCE_STDERR_LOGGING=1"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        TIMEOUT 90
        RUN_SERIAL TRUE
    )
endif()
