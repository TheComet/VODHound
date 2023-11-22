cmake_minimum_required (VERSION 3.21)
project (appliciation-gtk4
    LANGUAGES C
    VERSION 0.0.1)

find_package (GTK4 REQUIRED COMPONENTS
    gtk glib cairo pango harfbuzz gdk-pixbuf graphene)

gtk_compile_resource (
    "res/vodhound.gresource.xml"
    "src/vodhound.gresource.c")

add_executable (application-gtk4
    "res/vodhound.gresource.xml"

    "include/application/fighter_icons.h"
    "include/application/game_browser.h"

    "src/fighter_icons.c"
    "src/game_browser.c"

    "${PROJECT_BINARY_DIR}/src/vodhound.gresource.c"
    "src/main.c")
target_include_directories (application-gtk4
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries (application-gtk4
    PRIVATE
        VODHound::vh
        GTK4::glib
        GTK4::cairo
        GTK4::pango
        GTK4::harfbuzz
        GTK4::gdk-pixbuf
        GTK4::graphene
        GTK4::gtk)

set_target_properties (application-gtk4 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
    RUNTIME_OUTPUT_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_BINDIR}
    OUTPUT_NAME "VODHound-gtk4")

if (WIN32 OR CYGWIN)
    add_custom_command (TARGET application-gtk4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GTK4_RUNTIMES} ${VODHOUND_BUILD_BINDIR}
        COMMENT "Copying GTK4 DLLs to ${VODHOUND_BUILD_BINDIR}"
        VERBATIM)
endif ()

install (
    TARGETS application-gtk4
    DESTINATION ${VODHOUND_INSTALL_BINDIR})
