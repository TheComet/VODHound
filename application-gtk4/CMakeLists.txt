cmake_minimum_required (VERSION 3.21)
project (appliciation-gtk4
    LANGUAGES C
    VERSION 0.0.1)

find_package (PkgConfig REQUIRED)
pkg_check_modules (GTK4 REQUIRED IMPORTED_TARGET gtk4)

add_executable (application-gtk4
    "src/main.c")
target_include_directories (application-gtk4
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries (application-gtk4
    PRIVATE
        VODHound::vh
        PkgConfig::GTK4)

set_target_properties (application-gtk4 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
    RUNTIME_OUTPUT_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_BINDIR}
    OUTPUT_NAME "VODHound-gtk4")

install (
    TARGETS application-gtk4
    DESTINATION ${VODHOUND_INSTALL_BINDIR})
