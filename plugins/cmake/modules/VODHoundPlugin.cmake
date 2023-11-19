function (vodhound_add_plugin NAME)
    string (TOUPPER ${NAME} PLUGIN_NAME)
    string (REPLACE "-" "_" PLUGIN_NAME ${PLUGIN_NAME})
    string (REPLACE " " "_" PLUGIN_NAME ${PLUGIN_NAME})
    set (PLUGIN_NAME ${PLUGIN_NAME} PARENT_SCOPE)

    string (TOLOWER ${NAME} TARGET_NAME)
    string (REPLACE "_" "-" TARGET_NAME ${TARGET_NAME})
    string (REPLACE " " "-" TARGET_NAME ${TARGET_NAME})

    set (options "")
    set (oneValueArgs "")
    set (multiValueArgs
        SOURCES
        TESTS
        HEADERS
        DEFINES
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

        message (STATUS "Adding plugin: ${TARGET_NAME}")
        add_library (${TARGET_NAME} SHARED
            ${${PLUGIN_NAME}_SOURCES}
            ${${PLUGIN_NAME}_HEADERS})
        target_include_directories (${TARGET_NAME}
            PRIVATE
                ${${PLUGIN_NAME}_INCLUDES})
        target_compile_definitions (${TARGET_NAME}
            PRIVATE
                ${${PLUGIN_NAME}_DEFINES}
                PLUGIN_BUILDING
                "PLUGIN_VERSION=((${PROJECT_VERSION_MAJOR}<<24) | (${PROJECT_VERSION_MINOR}<<16) | (${PROJECT_VERSION_PATCH}<<8))"
                "PLUGIN_API=${PLUGIN_API}")
        target_link_libraries (${TARGET_NAME}
            PRIVATE
                ${${PLUGIN_NAME}_LIBS})
        set_target_properties (${TARGET_NAME}
            PROPERTIES
                PREFIX ""
                MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
                LIBRARY_OUTPUT_DIRECTORY "${VODHOUND_BUILD_PLUGINDIR}/${TARGET_NAME}"
                LIBRARY_OUTPUT_DIRECTORY_DEBUG "${VODHOUND_BUILD_PLUGINDIR}/${TARGET_NAME}"
                LIBRARY_OUTPUT_DIRECTORY_RELEASE "${VODHOUND_BUILD_PLUGINDIR}/${TARGET_NAME}"
                RUNTIME_OUTPUT_DIRECTORY "${VODHOUND_BUILD_PLUGINDIR}/${TARGET_NAME}"
                RUNTIME_OUTPUT_DIRECTORY_DEBUG "${VODHOUND_BUILD_PLUGINDIR}/${TARGET_NAME}"
                RUNTIME_OUTPUT_DIRECTORY_RELEASE "${VODHOUND_BUILD_PLUGINDIR}/${TARGET_NAME}"
                INSTALL_RPATH "${VODHOUND_INSTALL_LIBDIR}"
                VS_DEBUGGER_WORKING_DIRECTORY "${VODHOUND_BUILD_BINDIR}"
                VS_DEBUGGER_COMMAND "${VODHOUND_BUILD_BINDIR}/VODHound.exe")
        if (${PLUGIN_NAME}_DATA)
            set (${PLUGIN_NAME}_DATA_OUT)
            foreach (DATAFILE ${${PLUGIN_NAME}_DATA})
                set (in "${PROJECT_SOURCE_DIR}/${DATAFILE}")
                set (out "${VODHOUND_BUILD_DATADIR}/${TARGET_NAME}")
                list (APPEND ${PLUGIN_NAME}_DATA_OUT ${out})
                add_custom_command (
                    OUTPUT ${out}
                    DEPENDS ${in}
                    COMMAND ${CMAKE_COMMAND} -E copy ${in} ${out}
                    VERBATIM)
            endforeach ()
            add_custom_target (${PLUGIN_TARGET}-data ALL DEPENDS ${${PLUGIN_NAME}_DATA_OUT})
            install (
                FILES ${${TARGET_NAME}_DATA}
                DESTINATION "${VODHOUND_INSTALL_DATADIR}/${TARGET_NAME}/")
        endif ()
        if (VODHOUND_TESTS AND ${PLUGIN_NAME}_TESTS)
            enable_language (CXX)
            configure_file (${PLUGIN_TEST_MAIN_TEMPLATE} "tests/main.cpp" COPYONLY)
            add_executable (${TARGET_NAME}-tests
                ${${PLUGIN_NAME}_SOURCES}
                ${${PLUGIN_NAME}_HEADERS}
                ${${PLUGIN_NAME}_TESTS}
                "${PROJECT_BINARY_DIR}/tests/main.cpp")
            target_include_directories (${TARGET_NAME}-tests
                PRIVATE
                    ${${PLUGIN_NAME}_INCLUDES})
            target_compile_definitions (${TARGET_NAME}-tests
                PRIVATE
                    ${${PLUGIN_NAME}_DEFINES}
                    PLUGIN_BUILDING
                    "PLUGIN_VERSION=((${PROJECT_VERSION_MAJOR}<<24) | (${PROJECT_VERSION_MINOR}<<16) | (${PROJECT_VERSION_PATCH}<<8))"
                    "PLUGIN_API=${PLUGIN_API}")
            target_link_libraries (${TARGET_NAME}-tests
                PRIVATE
                    ${${PLUGIN_NAME}_LIBS}
                    gmock)
            set_target_properties (${TARGET_NAME}-tests
                PROPERTIES
                    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
                    RUNTIME_OUTPUT_DIRECTORY ${VODHOUND_BUILD_BINDIR}
                    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_BINDIR}
                    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_BINDIR}
                    INSTALL_RPATH ${VODHOUND_INSTALL_LIBDIR}
                    VS_DEBUGGER_WORKING_DIRECTORY ${VODHOUND_BUILD_BINDIR})
        endif ()
        set_property (
            DIRECTORY "${PROJECT_SOURCE_DIR}"
            PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME})
        if (TARGET application)
            add_dependencies (application ${TARGET_NAME})
        endif ()
        install (
            TARGETS ${TARGET_NAME}
            LIBRARY DESTINATION "${VODHOUND_INSTALL_PLUGINDIR}/${TARGET_NAME}/"
            RUNTIME DESTINATION "${VODHOUND_INSTALL_PLUGINDIR}/${TARGET_NAME}/")
    endif ()
endfunction ()
