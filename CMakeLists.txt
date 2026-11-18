cmake_minimum_required(VERSION 3.24)
project(FramelessKit VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)
endif()

if(NOT DEFINED CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 20)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep legacy command-line switches working for standalone builds only.
function(framelesskit_option name description default)
    if(PROJECT_IS_TOP_LEVEL AND DEFINED ${name})
        set(default "${${name}}")
    endif()
    option(FRAMELESSKIT_${name} "${description}" "${default}")
endfunction()

set(framelesskit_shared_default ON)
if(DEFINED BUILD_SHARED_LIBS)
    set(framelesskit_shared_default "${BUILD_SHARED_LIBS}")
endif()
option(FRAMELESSKIT_BUILD_SHARED_LIBS "Build FramelessKit shared libraries" "${framelesskit_shared_default}")
framelesskit_option(BUILD_WIDGETS "Build the Widgets adapter" ON)
framelesskit_option(BUILD_QUICK "Build the Quick adapter and QML module" ON)
framelesskit_option(BUILD_EXAMPLES "Build example applications" "${PROJECT_IS_TOP_LEVEL}")
framelesskit_option(BUILD_TESTING "Build FramelessKit tests" OFF)
option(FRAMELESSKIT_INSTALL "Generate FramelessKit install rules" "${PROJECT_IS_TOP_LEVEL}")
# Optional undocumented Windows compatibility path; modern DWM remains the default.
framelesskit_option(ENABLE_LEGACY_ACCENT "Enable Windows legacy Accent Blur" ON)

if(FRAMELESSKIT_BUILD_SHARED_LIBS)
    set(FRAMELESSKIT_LIBRARY_TYPE SHARED)
else()
    set(FRAMELESSKIT_LIBRARY_TYPE STATIC)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui)

if(FRAMELESSKIT_BUILD_WIDGETS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
endif()

if(FRAMELESSKIT_BUILD_QUICK)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Qml Quick)
endif()

qt_standard_project_setup(REQUIRES 6.8)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FramelessKitTargets.cmake")

# Keep standalone artifacts together; embedded builds honor the host's output paths.
foreach(kind RUNTIME LIBRARY ARCHIVE)
    if(PROJECT_IS_TOP_LEVEL OR NOT DEFINED CMAKE_${kind}_OUTPUT_DIRECTORY)
        if(kind STREQUAL "RUNTIME")
            set(CMAKE_${kind}_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
        else()
            set(CMAKE_${kind}_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
        endif()
    endif()
endforeach()
if(NOT DEFINED QT_QML_OUTPUT_DIRECTORY)
    set(QT_QML_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/qml")
endif()
set(FRAMELESSKIT_QML_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/qml")

add_subdirectory(src/core)

if(FRAMELESSKIT_BUILD_WIDGETS)
    add_subdirectory(src/widgets)
endif()

if(FRAMELESSKIT_BUILD_QUICK)
    add_subdirectory(src/quick)
endif()

if(FRAMELESSKIT_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(FRAMELESSKIT_BUILD_TESTING)
    if(PROJECT_IS_TOP_LEVEL)
        include(CTest)
    endif()
    enable_testing()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

if(FRAMELESSKIT_INSTALL)
    configure_package_config_file(cmake/FramelessKitConfig.cmake.in
        "${PROJECT_BINARY_DIR}/FramelessKitConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit"
        PATH_VARS FRAMELESSKIT_QML_INSTALL_DIR
    )

    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/FramelessKitConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMinorVersion
    )

    install(EXPORT FramelessKitCoreTargets NAMESPACE FramelessKit:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit")

    if(FRAMELESSKIT_BUILD_WIDGETS)
        install(EXPORT FramelessKitWidgetsTargets NAMESPACE FramelessKit:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit")
    endif()

    if(FRAMELESSKIT_BUILD_QUICK)
        install(EXPORT FramelessKitQuickTargets NAMESPACE FramelessKit:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit")
    endif()

    install(FILES
        "${PROJECT_BINARY_DIR}/FramelessKitConfig.cmake"
        "${PROJECT_BINARY_DIR}/FramelessKitConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit"
    )
endif()
