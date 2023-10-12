cmake_minimum_required (VERSION 3.21)

project ("vod-review"
    VERSION 0.0.1
    LANGUAGES C)

include (VODHoundPlugin)
vodhound_add_plugin (${PROJECT_NAME}
    SOURCES
        "src/plugin.c"
    HEADERS
    INCLUDES
        "include"
    LIBS
        VODHound::vh
        IUP::iup)

if (VODHOUND_VOD_REVIEW)
endif ()

