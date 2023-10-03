cmake_minimum_required (VERSION 3.21)

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

project ("FFMpeg Video"
    VERSION 0.0.1
    LANGUAGES C)

include (VODHoundPlugin)
vodhound_add_plugin (${PROJECT_NAME}
    SOURCES
        "src/plugin.c"
    HEADERS
    INCLUDES
        "include")

if (VODHOUND_FFMPEG_VIDEO AND FALSE)
    find_package (FFMPEG COMPONENTS avcodec avformat avutil avdevice swscale swresample REQUIRED)
    target_link_libraries (plugin-${PLUGIN_NAME}
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

            add_custom_command (TARGET plugin-${PLUGIN_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${REFRAMED_BUILD_PLUGINDEPSDIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL_FILENAME}" "${REFRAMED_BUILD_PLUGINDEPSDIR}"
                COMMENT "Copying FFMPEG DLL ${DLL_FILENAME} to runtime dir ${REFRAMED_BUILD_PLUGINDEPSDIR}"
                VERBATIM)
			install (
				FILES ${DLL_FILENAME}
				DESTINATION ${REFRAMED_INSTALL_PLUGINDEPSDIR})
        endforeach ()
    endif ()
endif ()