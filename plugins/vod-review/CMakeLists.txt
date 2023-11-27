cmake_minimum_required (VERSION 3.21)

project ("vod-review"
    VERSION 0.0.1
    LANGUAGES C)

include (VODHoundPlugin)

find_package (GTK4 REQUIRED COMPONENTS
    gtk glib cairo pango harfbuzz gdk-pixbuf graphene epoxy)

vodhound_add_plugin (${PROJECT_NAME}
    SOURCES
        "src/plugin_vod_review.c"
    HEADERS
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

if (VODHOUND_VOD_REVIEW)
endif ()
