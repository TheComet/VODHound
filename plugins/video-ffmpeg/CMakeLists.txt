cmake_minimum_required (VERSION 3.21)

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

project ("video-ffmpeg"
    VERSION 0.0.1
    LANGUAGES C)

include (VODHoundPlugin)
vodhound_add_plugin (${PROJECT_NAME}
    SOURCES
        "src/decoder.c"
        "src/plugin.c"
    HEADERS
        "include/${PROJECT_NAME}/canvas.h"
        "include/${PROJECT_NAME}/decoder.h"
        "include/${PROJECT_NAME}/gfx.h"
    INCLUDES
        "include")

if (VODHOUND_FFMPEG_VIDEO)
    if (CMAKE_PLATFORM_ID MATCHES Windows)
        set (DEFAULT_GFX "DX11")
        set (POSSIBLE_GFX "DX11;WGL")
    elseif (CMAKE_PLATFORM_ID MATCHES Linux)
        set (DEFAULT_GFX "GLX")
        set (POSSIBLE_GFX "GLX")
    endif ()
    message (FATAL_ERROR ".${DEFAULT_GFX}")
    set (VODHOUND_FFMPEG_VIDEO_GFX ${DEFAULT_GFX} CACHE STRING "Which graphics API to use for rendering")
    set_property (CACHE VODHOUND_FFMPEG_VIDEO_GFX PROPERTY STRINGS ${POSSIBLE_GFX})

    if (VODHOUND_FFMPEG_VIDEO_GFX MATCHES "DX11")
        target_sources (${PROJECT_NAME} PRIVATE
            "src/canvas_win32_dx.c"
            "src/gfx_dx11.c")
        target_link_libraries (${PROJECT_NAME} PRIVATE d3d11)
    elseif (VODHOUND_FFMPEG_VIDEO_GFX MATCHES "WGL")
        target_sources (${PROJECT_NAME} PRIVATE
            "src/canvas_win32_gl.c"
            "src/gfx_gl.c")
    elseif (VODHOUND_FFMPEG_VIDEO_GFX MATCHES "GLX")
        target_sources (${PROJECT_NAME} PRIVATE
            "src/canvas_glx.c"
            "src/gfx_gl.c")
    endif ()

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
