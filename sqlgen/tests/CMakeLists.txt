cmake_minimum_required (VERSION 3.21)

include (FetchContent)

project ("sqlgen_tests"
    LANGUAGES CXX
    VERSION 1.0.0)

FetchContent_Declare (
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        f8d7d77c06936315286eb55f8de22cd23c188571 # tag v1.14
)
FetchContent_Declare (
    sqlite3
    URL https://www.sqlite.org/2023/sqlite-amalgamation-3440200.zip
    URL_HASH SHA256=833be89b53b3be8b40a2e3d5fedb635080e3edb204957244f3d6987c2bb2345f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

add_subdirectory ("../sqlgen" "${PROJECT_BINARY_DIR}/sqlgen")
sqlgen_target (migrations
    INPUT "migrations.sqlgen"
    HEADER "sqlgen/tests/migrations.h"
    BACKENDS sqlite3)

add_executable (sqlgen_tests
    "test_migrations.cpp"
    ${SQLGEN_migrations_OUTPUTS})
target_include_directories (sqlgen_tests PRIVATE ${PROJECT_BINARY_DIR})
set_property(
    DIRECTORY ${PROJECT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT sqlgen_tests)

FetchContent_MakeAvailable (googletest)
target_link_libraries (sqlgen_tests PRIVATE gmock gmock_main)

FetchContent_MakeAvailable (sqlite3)
FetchContent_GetProperties (sqlite3)
add_library (sqlite3 STATIC
    "${sqlite3_SOURCE_DIR}/sqlite3.c")
target_include_directories (sqlite3
    PUBLIC
        "$<BUILD_INTERFACE:${sqlite3_SOURCE_DIR}>")
target_link_libraries (sqlite3
    PRIVATE
        $<$<PLATFORM_ID:Linux>:$<$<BOOL:${SQLITE_EXTENSIONS}>:dl>>
        $<$<PLATFORM_ID:Linux>:$<$<BOOL:${SQLITE_FTS5}>:m>>)
#if (CMAKE_SYSTEM_NAME MATCHES "Linux" OR CMAKE_SYSTEM_NAME MATCHES "Darwin")
#    find_package (Threads REQUIRED)
#    target_link_libraries (sqlite PRIVATE Threads::Threads)
#endif ()
target_link_libraries (sqlgen_tests PRIVATE sqlite3)
