project (vodhound-tests
    LANGUAGES CXX)

add_executable (vodhound-tests
    "src/main.cpp")
target_link_libraries (vodhound-tests
    PRIVATE
        gmock)
set_target_properties (vodhound-tests PROPERTIES
    CXX_STANDARD 17
    VS_DEBUGGER_WORKING_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
    RUNTIME_OUTPUT_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_BINDIR}
    OUTPUT_NAME "VODHound-tests")
