project (application
    LANGUAGES C
    VERSION ${VODHOUND_VERSION})

add_executable (application
    "src/main.c")
target_include_directories (application
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries (application
    PRIVATE
        vh)
        #iup
        #iupimglib)

set_target_properties (application PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
    RUNTIME_OUTPUT_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_BINDIR}
    OUTPUT_NAME "VODHound")
