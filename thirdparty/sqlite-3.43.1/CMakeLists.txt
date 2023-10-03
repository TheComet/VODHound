project (sqlite
    LANGUAGES C
    VERSION 3.43.1)

option (SQLITE_THREADS "Enable threading support" ON)
option (SQLITE_PIC "Enable position independent code" ON)
option (SQLITE_EXENSIONS "Enable loading extensions" OFF)
option (SQLITE_CLI "Build the CLI program" OFF)

add_library (sqlite
    "include/sqlite/sqlite3.h"
    "include/sqlite/sqlite3ext.h"
    "src/sqlite3.c")
target_include_directories (sqlite
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
set_target_properties (sqlite
    PROPERTIES
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
        POSITION_INDEPENDENT_CODE ${SQLITE_PIC})

if (CMAKE_PLATFORM_ID MATCHES "Linux" OR CMAKE_PLATFORM_ID MATCHES "Darwin")
    if (SQLITE_THREADS)
        find_package (Threads REQUIRED)
        target_link_libraries (sqlite PRIVATE Threads::Threads)
    else ()
        target_compile_definitions (sqlite PRIVATE SQLITE_THREADSAFE=0)
    endif ()

    if (SQLITE_EXTENSIONS)
        target_link_libraries (sqlite PRIVATE dl)
    else ()
        target_compile_definitions (sqlite PRIVATE SQLITE_OMIT_LOAD_EXTENSION)
    endif ()

    target_link_libraries (sqlite PRIVATE m)
endif ()

if (SQLITE_CLI)
    add_executable (sqlite_cli
        "src/shell.c")
    target_include_directories (sqlite_cli PRIVATE
        "include"
        "include/sqlite")
    target_link_libraries (sqlite_cli PRIVATE sqlite)
    set_target_properties (sqlite_cli PROPERTIES
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
        VS_DEBUGGER_WORKING_DIRECTORY ${VODHOUND_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY ${VODHOUND_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_BINDIR}
        OUTPUT_NAME "sqlite")

    find_package (Readline)
    find_package (Curses)
    if (CURSES_FOUND AND READLINE_FOUND)
        target_link_libraries (sqlite_cli PRIVATE ${READLINE_LIBRARY} ${CURSES_LIBRARIES})
        target_link_libraries (sqlite_cli PRIVATE Readline::Readline)
        target_compile_definitions (sqlite_cli PRIVATE HAVE_READLINE)
    endif ()
    
    install (
        TARGETS sqlite_cli
        RUNTIME DESTINATION ${VODHOUND_INSTALL_BINDIR})
endif ()

install (
    TARGETS sqlite
    EXPORT VODHoundTargets
    INCLUDES DESTINATION ${VODHOUND_INSTALL_INCLUDEDIR}
    ARCHIVE DESTINATION ${VODHOUND_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${VODHOUND_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${VODHOUND_INSTALL_BINDIR})
