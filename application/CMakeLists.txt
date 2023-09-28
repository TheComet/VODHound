project (application
    LANGUAGES C
    VERSION ${REFRAMED_VERSION})

add_executable (application
    "src/main.c")
target_include_directories (application
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries (application
    PRIVATE
        rf)

set_target_properties (application PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${REFRAMED_BUILD_BINDIR}
    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
    RUNTIME_OUTPUT_DIRECTORY ${REFRAMED_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${REFRAMED_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${REFRAMED_BUILD_BINDIR}
    OUTPUT_NAME "ReFramed")
