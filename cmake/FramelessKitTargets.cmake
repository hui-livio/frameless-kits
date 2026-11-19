function(framelesskit_configure_target target component)
    add_library(FramelessKit::${component} ALIAS ${target})
    set_target_properties(${target} PROPERTIES
        AUTOMOC ON
        EXPORT_NAME ${component}
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    string(TOUPPER "${component}" upper_component)
    if(FRAMELESSKIT_BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PRIVATE FRAMELESSKIT_BUILD_${upper_component})
    else()
        target_compile_definitions(${target} PUBLIC FRAMELESSKIT_STATIC)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4 /permissive-)
    endif()
    get_target_property(target_type ${target} TYPE)
    if(FRAMELESSKIT_INSTALL AND NOT target_type STREQUAL "OBJECT_LIBRARY")
        install(TARGETS ${target}
            EXPORT FramelessKit${component}Targets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        )
    endif()
endfunction()

# Embed Core objects without exposing an internal target in installed packages.
# Forward its native link dependencies explicitly, including for static consumers.
function(framelesskit_embed_core target)
    get_target_property(core_dependencies fk_core LINK_LIBRARIES)
    target_link_libraries(${target} PRIVATE $<BUILD_INTERFACE:fk_core> ${core_dependencies})
    target_link_libraries(${target} PUBLIC Qt6::Core Qt6::Gui)
    if(FRAMELESSKIT_BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PRIVATE FRAMELESSKIT_BUILD_CORE)
    endif()
endfunction()
