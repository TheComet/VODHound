cmake_minimum_required (VERSION 3.21)

project ("aitool"
    VERSION 0.0.1
    LANGUAGES C)

include (VODHoundPlugin)

find_package (GTK4 REQUIRED COMPONENTS
    gtk glib cairo pango harfbuzz gdk-pixbuf graphene epoxy)

vodhound_add_plugin (${PROJECT_NAME}
    SOURCES
        "src/plugin_aitool.c"
    HEADERS
    INCLUDES
        "include"
    LIBS
        VODHound::vh
        sqlite
        GTK4::glib
        GTK4::cairo
        GTK4::pango
        GTK4::harfbuzz
        GTK4::gdk-pixbuf
        GTK4::graphene
        GTK4::epoxy
        GTK4::gtk)

if (VODHOUND_AITOOL)
    sqlgen_target (aidb
        INPUT "src/aidb.sqlgen"
        HEADER "include/aitool/db.h"
        BACKENDS sqlite3)
    target_sources (${PROJECT_NAME} PRIVATE ${SQLGEN_aidb_OUTPUTS})
    target_include_directories (${PROJECT_NAME} PRIVATE "${PROJECT_BINARY_DIR}/include")
endif ()
