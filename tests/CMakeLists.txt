
function(windowheler_add_test target source)
    qt_add_executable(${target} ${source})
    set_target_properties(${target} PROPERTIES AUTOMOC ON)
    target_link_libraries(${target} PRIVATE Qt6::Test)
    if(ARGC GREATER 2)
        target_link_libraries(${target} PRIVATE ${ARGV2})
    else()
        # Internal tests exercise the common runtime directly.
        target_link_libraries(${target} PRIVATE WindowHeler::Runtime)
    endif()
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/core")
    add_test(NAME ${target} COMMAND ${target})
endfunction()

windowheler_add_test(windowheler_core_tests CoreTests.cpp)
windowheler_add_test(windowheler_runtime_tests RuntimeTests.cpp)

if(TARGET WindowHeler::FramelessWidgets)
    windowheler_add_test(windowheler_widgets_tests WidgetsTests.cpp WindowHeler::FramelessWidgets)
    if(TARGET WindowHeler::MaterialWidgets)
        target_link_libraries(windowheler_widgets_tests PRIVATE WindowHeler::MaterialWidgets)
        target_compile_definitions(windowheler_widgets_tests PRIVATE TEST_WINDOW_MATERIAL)
    endif()
endif()

if(TARGET WindowHeler::FramelessQuick)
    windowheler_add_test(windowheler_quick_tests QuickTests.cpp WindowHeler::FramelessQuick)
    if(TARGET WindowHeler::MaterialQuick)
        target_link_libraries(windowheler_quick_tests PRIVATE WindowHeler::MaterialQuick)
        target_compile_definitions(windowheler_quick_tests PRIVATE TEST_WINDOW_MATERIAL)
    endif()
endif()

if(TARGET windowheler_widgets_demo)
    add_test(NAME windowheler_widgets_demo_smoke COMMAND windowheler_widgets_demo --smoke-test)
endif()

if(TARGET windowheler_quick_demo)
    add_test(NAME windowheler_qml_demo_smoke COMMAND windowheler_quick_demo --smoke-test)
endif()

if(PROJECT_IS_TOP_LEVEL AND WINDOWHELER_INSTALL AND WINDOWHELER_BUILD_FRAMELESS AND WINDOWHELER_BUILD_MATERIAL
   AND (WINDOWHELER_BUILD_WIDGETS OR WINDOWHELER_BUILD_QUICK))
    add_test(NAME windowheler_installed_consumer
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
            "-DWIDGETS=${WINDOWHELER_BUILD_WIDGETS}"
            "-DQUICK=${WINDOWHELER_BUILD_QUICK}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/RunInstalledConsumer.cmake"
    )
endif()

get_property(test_names DIRECTORY PROPERTY TESTS)

set_tests_properties(${test_names} PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
    TIMEOUT 30
)

if(TEST windowheler_installed_consumer)
    set_tests_properties(windowheler_installed_consumer PROPERTIES TIMEOUT 180)
endif()

# Compile the source header without any WindowHeler target or generated includes.
foreach(mode default static runtime framelesswidgets framelessquick materialwidgets materialquick)
    set(check_target windowheler_export_${mode}_check)
    add_library(${check_target} OBJECT ExportHeaderChecks.cpp)
    set_target_properties(${check_target} PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    target_compile_features(${check_target} PRIVATE cxx_std_20)
    target_include_directories(${check_target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(${check_target} PRIVATE Qt6::Core)
    string(TOUPPER "${mode}" upper_mode)
    if(mode STREQUAL "static")
        target_compile_definitions(${check_target} PRIVATE WINDOWHELER_STATIC)
    elseif(NOT mode STREQUAL "default")
        target_compile_definitions(${check_target} PRIVATE WINDOWHELER_BUILD_${upper_mode})
    endif()
endforeach()

if(WIN32 AND TARGET WindowHeler::FramelessWidgets)
    windowheler_add_test(windowheler_widgets_native_tests WidgetsNativeTests.cpp WindowHeler::FramelessWidgets)
    target_link_libraries(windowheler_widgets_native_tests PRIVATE user32)
    if(TARGET WindowHeler::MaterialWidgets)
        target_link_libraries(windowheler_widgets_native_tests PRIVATE WindowHeler::MaterialWidgets)
        target_compile_definitions(windowheler_widgets_native_tests PRIVATE TEST_WINDOW_MATERIAL)
    endif()
    set_tests_properties(windowheler_widgets_native_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=windows;QT_FORCE_STDERR_LOGGING=1"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        TIMEOUT 90
        RUN_SERIAL TRUE
    )
endif()

if(WIN32 AND TARGET WindowHeler::FramelessQuick)
    windowheler_add_test(windowheler_quick_native_tests QuickNativeTests.cpp WindowHeler::FramelessQuick)
    target_link_libraries(windowheler_quick_native_tests PRIVATE user32 gdi32)
    if(TARGET WindowHeler::MaterialQuick)
        target_link_libraries(windowheler_quick_native_tests PRIVATE WindowHeler::MaterialQuick)
        target_compile_definitions(windowheler_quick_native_tests PRIVATE TEST_WINDOW_MATERIAL
            WINDOWHELER_TEST_QML_IMPORT_PATH="${QT_QML_OUTPUT_DIRECTORY}")
    endif()
    set_tests_properties(windowheler_quick_native_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=windows;QT_FORCE_STDERR_LOGGING=1"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        TIMEOUT 90
        RUN_SERIAL TRUE
    )
endif()
