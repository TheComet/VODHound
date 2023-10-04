function (vodhound_add_plugin NAME)
    string (TOUPPER ${NAME} PLUGIN_NAME)
    string (REPLACE "-" "_" PLUGIN_NAME ${PLUGIN_NAME})
    string (REPLACE " " "_" PLUGIN_NAME ${PLUGIN_NAME})
    
    string (TOLOWER ${NAME} TARGET_NAME)
    string (REPLACE "_" "-" TARGET_NAME ${TARGET_NAME})
    string (REPLACE " " "-" TARGET_NAME ${TARGET_NAME})
    
    message (STATUS "name: ${PLUGIN_NAME}")
    message (STATUS "target: ${TARGET_NAME}")
    
    set (options "")
    set (oneValueArgs "")
    set (multiValueArgs
        SOURCES 
        HEADERS 
        INCLUDES
        LIBS
        DATA)
    cmake_parse_arguments (${PLUGIN_NAME} "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    option (VODHOUND_${PLUGIN_NAME} "Enable the ${TARGET_NAME} plugin" ON)
    
    if (${VODHOUND_${PLUGIN_NAME}})
        include (TestVisibilityMacros)
        check_c_source_compiles ("__declspec(dllexport) void foo(void); int main(void) { return 0; }" DLLEXPORT_VISIBILITY)
        check_c_source_compiles ("__attribute__((visibility(\"default\"))) void foo(void); int main(void) { return 0; }" DEFAULT_VISIBILITY)
        if (DLLEXPORT_VISIBILITY)
            set (PLUGIN_API "__declspec(dllexport)")
        elseif (DEFAULT_VISIBILITY)
            set (PLUGIN_API "__attribute__((visibility(\"default\")))")
        else ()
            message (FATAL_ERROR "Don't know how to define visibility macros for this compiler")
        endif ()
        
        add_library (${TARGET_NAME} SHARED
            ${${PLUGIN_NAME}_SOURCES}
            ${${PLUGIN_NAME}_HEADERS})
        target_include_directories (${TARGET_NAME}
            PRIVATE
                ${${PLUGIN_NAME}_INCLUDES})
        target_compile_definitions (${TARGET_NAME}
            PRIVATE
                PLUGIN_BUILDING
                "PLUGIN_VERSION=((${PROJECT_VERSION_MAJOR}<<24) | (${PROJECT_VERSION_MINOR}<<16) | (${PROJECT_VERSION_PATCH}<<8))"
                "PLUGIN_API=${PLUGIN_API}")
        target_link_libraries (${TARGET_NAME}
            PRIVATE
                VODHound::vh)
        set_target_properties (${TARGET_NAME}
            PROPERTIES
                PREFIX ""
                LIBRARY_OUTPUT_DIRECTORY "${VODHOUND_BUILD_PLUGINDIR}"
                LIBRARY_OUTPUT_DIRECTORY_DEBUG "${VODHOUND_BUILD_PLUGINDIR}"
                LIBRARY_OUTPUT_DIRECTORY_RELEASE "${VODHOUND_BUILD_PLUGINDIR}"
                RUNTIME_OUTPUT_DIRECTORY "${VODHOUND_BUILD_PLUGINDIR}"
                RUNTIME_OUTPUT_DIRECTORY_DEBUG "${VODHOUND_BUILD_PLUGINDIR}"
                RUNTIME_OUTPUT_DIRECTORY_RELEASE "${VODHOUND_BUILD_PLUGINDIR}"
                INSTALL_RPATH "${VODHOUND_INSTALL_LIBDIR}"
                VS_DEBUGGER_WORKING_DIRECTORY "${VODHOUND_BUILD_BINDIR}"
                VS_DEBUGGER_COMMAND "${VODHOUND_BUILD_BINDIR}/VODHound.exe")
        if (${PLUGIN_NAME}_DATA)
            foreach (DATAFILE ${${PLUGIN_NAME}_DATA})
                add_custom_command (TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${VODHOUND_BUILD_DATADIR}/${TARGET_NAME}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/${DATAFILE}" "${VODHOUND_BUILD_DATADIR}/${TARGET_NAME}"
                    COMMENT "Copying '${DATAFILE}' to '${VODHOUND_BUILD_DATADIR}/${TARGET_NAME}'"
                    VERBATIM)
            endforeach ()
            install (
                FILES ${${TARGET_NAME}_DATA}
                DESTINATION "${VODHOUND_INSTALL_DATADIR}/${TARGET_NAME}")
        endif ()
        set_property (
            DIRECTORY "${PROJECT_SOURCE_DIR}"
            PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME})
        install (
            TARGETS ${TARGET_NAME}
            LIBRARY DESTINATION "${VODHOUND_INSTALL_PLUGINDIR}"
            RUNTIME DESTINATION "${VODHOUND_INSTALL_PLUGINDIR}")
    endif ()
endfunction ()
