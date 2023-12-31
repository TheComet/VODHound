cmake_minimum_required (VERSION 3.21)

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

project ("video-ffmpeg"
    VERSION 0.0.1
    LANGUAGES C)

include (VODHoundPlugin)

find_package (GTK4 REQUIRED COMPONENTS
    gtk glib cairo pango harfbuzz gdk-pixbuf graphene epoxy)

vodhound_add_plugin (${PROJECT_NAME}
    SOURCES
        "src/canvas.c"
        "src/decoder.c"
        "src/plugin_video_ffmpeg.c"
    HEADERS
        "include/${PROJECT_NAME}/canvas.h"
        "include/${PROJECT_NAME}/decoder.h"
        "include/${PROJECT_NAME}/gfx.h"
    INCLUDES
        "include"
    LIBS
        VODHound::vh
        GTK4::glib
        GTK4::cairo
        GTK4::pango
        GTK4::harfbuzz
        GTK4::gdk-pixbuf
        GTK4::graphene
        GTK4::epoxy
        GTK4::gtk)

if (VODHOUND_VIDEO_FFMPEG)
    # NOTE: dx11 backend disabled for now, GTK only supports GL
    target_sources (${PROJECT_NAME} PRIVATE "src/gfx_gl.c")

    find_package (FFMPEG COMPONENTS avcodec avformat avutil avdevice swscale swresample REQUIRED)
    target_link_libraries (${PROJECT_NAME}
        PRIVATE FFMPEG::avutil
        PRIVATE FFMPEG::avcodec
        PRIVATE FFMPEG::avformat
        PRIVATE FFMPEG::avdevice
        PRIVATE FFMPEG::swresample
        PRIVATE FFMPEG::swscale)

    # On Windows we have to copy the DLLs to our binary directory
    if (WIN32 OR CYGWIN)
        foreach (LIB ${FFMPEG_LIBRARIES})
            get_filename_component (LIB_FILENAME ${LIB} NAME_WE)
            get_filename_component (FFMPEG_BIN_DIR ${LIB} DIRECTORY)
            get_filename_component (FFMPEG_BIN_DIR "${FFMPEG_BIN_DIR}/../bin" ABSOLUTE)

            file (GLOB DLL_FILENAMES "${FFMPEG_BIN_DIR}/${LIB_FILENAME}*.dll")
            list (GET DLL_FILENAMES 0 DLL_FILENAME)
            if (NOT DLL_FILENAME)
                message (FATAL_ERROR "Failed to find DLL matching library ${LIB}")
            endif ()

            add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL_FILENAME}" "${VODHOUND_BUILD_PLUGINDIR}/${PROJECT_NAME}"
                COMMENT "Copying FFMPEG DLL ${DLL_FILENAME} to runtime dir ${VODHOUND_BUILD_PLUGINDIR}/${PROJECT_NAME}"
                VERBATIM)
            install (
                FILES ${DLL_FILENAME}
                DESTINATION "${VODHOUND_INSTALL_PLUGINDIR}/${PROJECT_NAME}")
        endforeach ()
    endif ()
endif ()
