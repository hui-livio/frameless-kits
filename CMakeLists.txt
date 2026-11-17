cmake_minimum_required(VERSION 3.24)
project(FramelessKit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build FramelessKit shared libraries" ON)
option(BUILD_WIDGETS "Build the Widgets adapter" ON)
option(BUILD_QUICK "Build the Quick adapter and QML module" ON)
option(BUILD_EXAMPLES "Build example applications" ON)
# Reserved for the subsequent native material implementation; OFF in the scaffold.
option(ENABLE_LEGACY_ACCENT "Enable the future Windows legacy Accent backend" OFF)
if(BUILD_SHARED_LIBS)
    set(FRAMELESSKIT_LIBRARY_TYPE SHARED)
else()
    set(FRAMELESSKIT_LIBRARY_TYPE STATIC)
endif()
include(CTest)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui)
if(BUILD_WIDGETS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
endif()
if(BUILD_QUICK)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Qml Quick)
endif()
qt_standard_project_setup(REQUIRES 6.8)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/FramelessKitTargets.cmake)

# A common output root also makes Windows build-tree demos/tests runnable.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(QT_QML_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/qml")
set(FRAMELESSKIT_QML_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/qml")

add_subdirectory(src/core)
if(BUILD_WIDGETS)
    add_subdirectory(src/widgets)
endif()
if(BUILD_QUICK)
    add_subdirectory(src/quick)
endif()
if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

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
if(BUILD_WIDGETS)
    install(EXPORT FramelessKitWidgetsTargets NAMESPACE FramelessKit:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit")
endif()
if(BUILD_QUICK)
    install(EXPORT FramelessKitQuickTargets NAMESPACE FramelessKit:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit")
endif()
install(FILES
    "${PROJECT_BINARY_DIR}/FramelessKitConfig.cmake"
    "${PROJECT_BINARY_DIR}/FramelessKitConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FramelessKit"
)
