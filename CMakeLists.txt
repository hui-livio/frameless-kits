cmake_minimum_required(VERSION 3.24)
project(WindowHeler VERSION 0.1.0 LANGUAGES CXX)

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
function(windowheler_option name description default)
    if(PROJECT_IS_TOP_LEVEL AND DEFINED ${name})
        set(default "${${name}}")
    endif()
    option(WINDOWHELER_${name} "${description}" "${default}")
endfunction()

set(windowheler_shared_default ON)
if(DEFINED BUILD_SHARED_LIBS)
    set(windowheler_shared_default "${BUILD_SHARED_LIBS}")
endif()
option(WINDOWHELER_BUILD_SHARED_LIBS "Build WindowHeler shared libraries" "${windowheler_shared_default}")
windowheler_option(BUILD_FRAMELESS "Build the Frameless capability" ON)
windowheler_option(BUILD_MATERIAL "Build the Material capability" ON)
windowheler_option(BUILD_WIDGETS "Build the Widgets frontends" ON)
windowheler_option(BUILD_QUICK "Build the Quick frontends and QML modules" ON)
windowheler_option(BUILD_EXAMPLES "Build example applications" "${PROJECT_IS_TOP_LEVEL}")
windowheler_option(BUILD_TESTING "Build WindowHeler tests" OFF)
option(WINDOWHELER_INSTALL "Generate WindowHeler install rules" "${PROJECT_IS_TOP_LEVEL}")
# Optional undocumented Windows compatibility path; modern DWM remains the default.
windowheler_option(ENABLE_LEGACY_ACCENT "Enable Windows legacy Accent Blur" ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    option(WINDOWHELER_ENABLE_X11 "Enable the native X11 window menu when available" ON)
    option(WINDOWHELER_ENABLE_WAYLAND "Enable the native Wayland window menu when available" ON)
endif()
set(WINDOWHELER_PRIVATE_QT_VERSION "")

if(WINDOWHELER_BUILD_SHARED_LIBS)
    set(WINDOWHELER_LIBRARY_TYPE SHARED)
else()
    set(WINDOWHELER_LIBRARY_TYPE STATIC)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui)

if(WINDOWHELER_BUILD_WIDGETS AND (WINDOWHELER_BUILD_FRAMELESS OR WINDOWHELER_BUILD_MATERIAL))
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
endif()

if(WINDOWHELER_BUILD_QUICK AND (WINDOWHELER_BUILD_FRAMELESS OR WINDOWHELER_BUILD_MATERIAL))
    find_package(Qt6 6.8 REQUIRED COMPONENTS Qml Quick)
endif()

qt_standard_project_setup(REQUIRES 6.8)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/WindowHelerTargets.cmake")

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
set(WINDOWHELER_QML_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/qml")

add_subdirectory(src/core)

if(WINDOWHELER_BUILD_WIDGETS)
    add_subdirectory(src/widgets)
endif()

if(WINDOWHELER_BUILD_QUICK)
    add_subdirectory(src/quick)
endif()

if(WINDOWHELER_BUILD_EXAMPLES AND WINDOWHELER_BUILD_FRAMELESS AND WINDOWHELER_BUILD_MATERIAL)
    add_subdirectory(example)
endif()

if(WINDOWHELER_BUILD_TESTING)
    if(PROJECT_IS_TOP_LEVEL)
        include(CTest)
    endif()
    enable_testing()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

if(WINDOWHELER_INSTALL)
    configure_package_config_file(cmake/WindowHelerConfig.cmake.in
        "${PROJECT_BINARY_DIR}/WindowHelerConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WindowHeler"
        PATH_VARS WINDOWHELER_QML_INSTALL_DIR
    )

    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/WindowHelerConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMinorVersion
    )

    install(EXPORT WindowHelerTargets
        NAMESPACE WindowHeler::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WindowHeler"
    )

    install(FILES
        "${PROJECT_BINARY_DIR}/WindowHelerConfig.cmake"
        "${PROJECT_BINARY_DIR}/WindowHelerConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WindowHeler"
    )
endif()
