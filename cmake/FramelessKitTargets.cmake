function(framelesskit_configure_target target component)
    add_library(FramelessKit::${component} ALIAS ${target})
    set_target_properties(${target} PROPERTIES
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
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PRIVATE FRAMELESSKIT_BUILD_${upper_component})
    else()
        target_compile_definitions(${target} PUBLIC FRAMELESSKIT_STATIC)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4 /permissive-)
    endif()
    install(TARGETS ${target}
        EXPORT FramelessKit${component}Targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
endfunction()
