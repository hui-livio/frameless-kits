function(windowheler_configure_target target component)
    add_library(WindowHeler::${component} ALIAS ${target})
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
    if(WINDOWHELER_BUILD_SHARED_LIBS OR component STREQUAL "Runtime")
        target_compile_definitions(${target} PRIVATE WINDOWHELER_BUILD_${upper_component})
        if(component MATCHES "Widgets$")
            target_compile_definitions(${target} PRIVATE WINDOWHELER_BUILD_WIDGETS)
        elseif(component MATCHES "Quick$")
            target_compile_definitions(${target} PRIVATE WINDOWHELER_BUILD_QUICK)
        endif()
    else()
        target_compile_definitions(${target} PUBLIC WINDOWHELER_STATIC)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4 /permissive-)
    endif()
    get_target_property(target_type ${target} TYPE)
    if(WINDOWHELER_INSTALL AND NOT target_type STREQUAL "OBJECT_LIBRARY")
        install(TARGETS ${target}
            EXPORT WindowHelerTargets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        )
    endif()
endfunction()

function(windowheler_link_runtime target)
    target_link_libraries(${target} PUBLIC WindowHeler::Runtime)
    target_link_libraries(${target} PUBLIC Qt6::Core Qt6::Gui)
endfunction()
