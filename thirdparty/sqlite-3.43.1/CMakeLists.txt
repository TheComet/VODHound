cmake_minimum_required (VERSION 3.21)

project (sqlite
    LANGUAGES C
    VERSION 3.43.1)

set (DEBUG_OPTION OFF)
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    set (DEBUG_OPTION ON)
endif ()

option (SQLITE_ALLOCA "Use alloca() memory allocator in a few situations where it is appropriate. This results in a slightly smaller and faster binary. The SQLITE_USE_ALLOCA compile-time only works, of course, on systems that support alloca()." ON)
option (SQLITE_API_ARMOR "Activate extra code for detecting misuse of the SQLite API" ${DEBUG_OPTION})
option (SQLITE_AUTOINIT "For backwards compatibility with older versions of SQLite that lack the sqlite3_initialize() interface, the sqlite3_initialize() interface is called automatically upon entry to certain key interfaces such as sqlite3_open(), sqlite3_vfs_register(), and sqlite3_mprintf(). The overhead of invoking sqlite3_initialize() automatically in this way may be omitted by building SQLite with the SQLITE_OMIT_AUTOINIT C-preprocessor macro. When built using SQLITE_OMIT_AUTOINIT, SQLite will not automatically initialize itself and the application is required to invoke sqlite3_initialize() directly prior to beginning use of the SQLite library." OFF)
option (SQLITE_CLI "Build the CLI program" ${DEBUG_OPTION})
option (SQLITE_DEBUG "Enable assertions and debug checks" ${DEBUG_OPTION})
option (SQLITE_DECLTYPE "Add support for the sqlite3_column_decltype() and sqlite3_column_decltype16() interfaces." OFF)
option (SQLITE_DEPRECATED "Include support for interfaces marked as deprecated." OFF)
option (SQLITE_DQS "Enable support for double-quoted string literals" OFF)
option (SQLITE_EXENSIONS "Enable loading dynamic libraries as extensions" OFF)
option (SQLITE_FTS5 "Include version 5 of the full-text search engine" ON)
option (SQLITE_JSON "Include JSON SQL functions" OFF)
option (SQLITE_LIKE_DOESNT_MATCH_BLOBS "Historically, SQLite has allowed BLOB operands to the LIKE and GLOB operators. But having a BLOB as an operand of LIKE or GLOB complicates and slows the LIKE optimization. When this option is set, it means that the LIKE and GLOB operators always return FALSE if either operand is a BLOB. That simplifies the implementation of the LIKE optimization and allows queries that use the LIKE optimization to run faster." ON)
option (SQLITE_PIC "Enable position independent code" ON)
option (SQLITE_PROGRESS_CALLBACK "Include the sqlite3_progres_handler() API function" OFF)
option (SQLITE_THREADSAFE "Setting -DSQLITE_THREADSAFE=0 causes all of the mutex and thread-safety logic in SQLite to be omitted. This is the single compile-time option causes SQLite to run about 2% faster and also reduces the size of the library by about 2%. But the downside is that using the compile-time option means that SQLite can never be used by more than a single thread at a time, even if each thread has its own database connection." OFF)
option (SQLITE_SHARED_CACHE "Include support for shared cache mode. The sqlite3_enable_shared_cache() is omitted along with a fair amount of logic within the B-Tree subsystem associated with shared cache management." OFF)
set (SQLITE_MAX_EXPR_DEPTH 0 CACHE STRING "The SQLITE_MAX_EXPR_DEPTH parameter determines the maximum expression tree depth. If the value is 0, then no limit is enforced. The current implementation has a default value of 1000.")
set (SQLITE_DEFAULT_MEMSTATUS "0" CACHE STRING "This setting causes the sqlite3_status() interfaces that track memory usage to be disabled. This helps the sqlite3_malloc() routines run much faster, and since SQLite uses sqlite3_malloc() internally, this helps to make the entire library faster.")
set_property (CACHE SQLITE_DEFAULT_MEMSTATUS PROPERTY STRINGS "0;1")
set (SQLITE_DEFAULT_WAL_SYNCHRONOUS "1" CACHE STRING "For maximum database safety following a power loss, the setting of PRAGMA synchronous=FULL is recommended. However, in WAL mode, complete database integrity is guaranteed with PRAGMA synchronous=NORMAL. With PRAGMA synchronous=NORMAL in WAL mode, recent changes to the database might be rolled back by a power loss, but the database will not be corrupted. Furthermore, transaction commit is much faster in WAL mode using synchronous=NORMAL than with the default synchronous=FULL. For these reasons, it is recommended that the synchronous setting be changed from FULL to NORMAL when switching to WAL mode. This compile-time option will accomplish that.")
set_property (CACHE SQLITE_DEFAULT_WAL_SYNCHRONOUS PROPERTY STRINGS "0;1;2;3")

include (TestBigEndian)
test_big_endian(I_AM_BIG)

add_library (sqlite_def INTERFACE)
target_compile_definitions (sqlite_def
    INTERFACE
        $<$<BOOL:${I_AM_BIG}>:SQLITE_BYTEORDER=4321>
        $<$<NOT:$<BOOL:${I_AM_BIG}>>:SQLITE_BYTEORDER=1234>
        "SQLITE_DEFAULT_MEMSTATUS=${SQLITE_DEFAULT_MEMSTATUS}"
        "SQLITE_DEFAULT_WAL_SYNCHRONOUS=${SQLITE_DEFAULT_WAL_SYNCHRONOUS}"
        "SQLITE_MAX_EXPR_DEPTH=${SQLITE_MAX_EXPR_DEPTH}"
        $<$<BOOL:${SQLITE_API_ARMOR}>:SQLITE_ENABLE_API_ARMOR>
        $<$<BOOL:${SQLITE_DEBUG}>:SQLITE_DEBUG>
        $<$<BOOL:${SQLITE_FTS5}>:SQLITE_ENABLE_FTS5>
        $<$<BOOL:${SQLITE_LIKE_DOESNT_MATCH_BLOBS}>:SQLITE_LIKE_DOESNT_MATCH_BLOBS>
        $<$<BOOL:${SQLITE_ALLOCA}>:SQLITE_USE_ALLOCA>
        $<$<NOT:$<BOOL:${SQLITE_DECLTYPE}>>:SQLITE_OMIT_DECLTYPE>
        $<$<NOT:$<BOOL:${SQLITE_DEPRECATED}>>:SQLITE_OMIT_DEPRECATED>
        $<$<NOT:$<BOOL:${SQLITE_SHARED_CACHE}>>:SQLITE_OMIT_SHARED_CACHE>
        $<$<NOT:$<BOOL:${SQLITE_JSON}>>:SQLITE_OMIT_JSON>
        $<$<NOT:$<BOOL:${SQLITE_THREADSAFE}>>:SQLITE_THREADSAFE=0>
        $<$<NOT:$<BOOL:${SQLITE_PROGRESS_CALLBACK}>>:SQLITE_OMIT_PROGRESS_CALLBACK>
        $<$<NOT:$<BOOL:${SQLITE_DQS}>>:SQLITE_DQS=0>
        $<$<NOT:$<BOOL:${SQLITE_AUTOINIT}>>:SQLITE_OMIT_AUTOINIT>
        $<$<NOT:$<BOOL:${SQLITE_EXENSIONS}>>:SQLITE_OMIT_LOAD_EXTENSION>)

add_library (sqlite
    "include/sqlite/sqlite3.h"
    "include/sqlite/sqlite3ext.h"
    "src/sqlite3.c")
target_include_directories (sqlite
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries (sqlite
    PRIVATE
        sqlite_def
        $<$<PLATFORM_ID:Linux>:$<$<BOOL:${SQLITE_EXTENSIONS}>:dl>>
        $<$<PLATFORM_ID:Linux>:$<$<BOOL:${SQLITE_FTS5}>:m>>)
set_target_properties (sqlite
    PROPERTIES
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
        POSITION_INDEPENDENT_CODE ${SQLITE_PIC})

if (CMAKE_SYSTEM_NAME MATCHES "Linux" OR CMAKE_SYSTEM_NAME MATCHES "Darwin")
    if (NOT SQLITE_THREADSAFE)
        find_package (Threads REQUIRED)
        target_link_libraries (sqlite PRIVATE Threads::Threads)
    endif ()
endif ()

if (SQLITE_CLI)
    add_executable (sqlite_cli
        "src/shell.c")
    target_include_directories (sqlite_cli PRIVATE
        "include"
        "include/sqlite")
    target_link_libraries (sqlite_cli PRIVATE sqlite_def sqlite)
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
    TARGETS sqlite sqlite_def
    EXPORT VODHoundTargets
    INCLUDES DESTINATION ${VODHOUND_INSTALL_INCLUDEDIR}
    ARCHIVE DESTINATION ${VODHOUND_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${VODHOUND_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${VODHOUND_INSTALL_BINDIR})
