include (CheckCSourceCompiles)

project ("vh"
    VERSION ${VODHOUND_VERSION}
    LANGUAGES C)

###############################################################################
# Options and config
###############################################################################

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set (DEBUG_FEATURE ON)
endif ()

set (VH_LIB_TYPE "SHARED" CACHE STRING "Build as shared or static")
set (VH_BTREE_EXPAND_FACTOR "2" CACHE STRING "When reallocating btree memory, this is the factor with which the buffer grows")
set (VH_BTREE_MIN_CAPACITY "32" CACHE STRING "The smallest number of elements to reserve when initializing a btree")
option (VH_BTREE_64BIT_KEYS "Enable 64-bit keys for btrees instead of 32-bit keys" OFF)
option (VH_BTREE_64BIT_CAPACITY "Enable btrees to allow up to 2^64 entries instead of 2^32" OFF)
option (VH_HM_STATS "Track hashmap usage statistics. This will increase sizeof(struct hm)!" ${DEBUG_FEATURE})
set (VH_HM_REHASH_AT_PERCENT "70" CACHE STRING "How full the hash table needs to be before triggering a rehash, in percent")
set (VH_HM_MIN_CAPACITY "128" CACHE STRING "Default table size when creating new hashmaps")
set (VH_HM_EXPAND_FACTOR "4" CACHE STRING "When reallocating hashmap memory, thiis is the factor with which the buffer grows. WARNING: MUST be a power of 2!")
option (VH_MEM_BACKTRACE "Enable generating backtraces to every malloc/realloc call, making it easy to find where memory leaks occur" ${DEBUG_FEATURE})
set (VH_MEM_BACKTRACE_SIZE "64" CACHE STRING "Sets the maximum stack size (depth) when generating backtraces")
option (VH_MEM_DEBUGGING "Enable tracking malloc/realloc/free calls to detect memory leaks" ${DEBUG_FEATURE})
option (VH_PROFILING "Enable -pg and -fno-omit-frame-pointer" OFF)
option (VH_VEC_64BIT_CAPACITY "Set vector capacity to 2^64 instead of 2^32, but makes the structure 32 bytes instead of 20 bytes" OFF)
set (VH_VEC_EXPAND_FACTOR "2" CACHE STRING "When reallocating vector memory, this is the factor with which the buffer grows")
set (VH_VEC_MIN_CAPACITY "32" CACHE STRING "The smallest number of elements to reserve when initializing a vector")

# Visibility
check_c_source_compiles ("__declspec(dllexport) void foo(void); int main(void) { return 0; }" DLLEXPORT_VISIBILITY)
check_c_source_compiles ("__declspec(dllimport) void foo(void); int main(void) { return 0; }" DLLIMPORT_VISIBILITY)
check_c_source_compiles ("__attribute__((visibility(\"default\"))) void foo(void); int main(void) { return 0; }" DEFAULT_VISIBILITY)
check_c_source_compiles ("__attribute__((visibility(\"hidden\"))) void foo(void); int main(void) { return 0; }"  HIDDEN_VISIBILITY)
if (DLLEXPORT_VISIBILITY AND DLLIMPORT_VISIBILITY)
    set (VH_API_EXPORT "__declspec(dllexport)")
    set (VH_API_IMPORT "__declspec(dllimport)")
    set (VH_API_LOCAL "")
elseif (DEFAULT_VISIBILITY AND HIDDEN_VISIBILITY)
    set (VH_API_EXPORT "__attribute__((visibility(\"default\")))")
    set (VH_API_IMPORT "__attribute__((visibility(\"default\")))")
    set (VH_API_LOCAL  "__attribute__((visibility(\"hidden\")))")
else ()
    message (FATAL_ERROR "Don't know how to define visibility macros for this compiler")
endif ()

# Memory debugging uses thread-local storage for its state because we don't want to deal with locks
check_c_source_compiles ("__declspec(thread) int value; int main(void) { return 0; }" MSVC_THREADLOCAL)
check_c_source_compiles ("__thread int value; int main(void) { return 0; }" GCC_THREADLOCAL)
if (MSVC_THREADLOCAL)
    set (VH_THREADLOCAL "__declspec(thread)")
endif ()
if (GCC_THREADLOCAL)
    set (VH_THREADLOCAL "__thread")
endif ()

configure_file ("templates/config.h.in" "include/vh/config.h")

###############################################################################
# Library source files and settings
###############################################################################

add_library (vh ${VH_LIB_TYPE}
    "templates/config.h.in"
    "${PROJECT_BINARY_DIR}/include/vh/config.h"

    "include/vh/backtrace.h"
    "include/vh/btree.h"
    "include/vh/cli_colors.h"
    "include/vh/db_ops.h"
    "include/vh/dynlib.h"
    "include/vh/fs.h"
    "include/vh/hash.h"
    "include/vh/hm.h"
    "include/vh/import.h"
    "include/vh/init.h"
    "include/vh/log.h"
    "include/vh/mem.h"
    "include/vh/mfile.h"
    "include/vh/mstream.h"
    "include/vh/plugin.h"
    "include/vh/plugin_loader.h"
    "include/vh/rb.h"
    "include/vh/str.h"
    "include/vh/thread.h"
    "include/vh/utf8.h"
    "include/vh/vec.h"

    "src/btree.c"
    "src/db_ops.c"
    "src/fs_common.c"
    "src/hash.c"
    "src/hm.c"
    "src/init.c"
    "src/log.c"
    "src/mem.c"
    "src/mstream.c"
    "src/plugin_loader.c"
    "src/rb.c"
    "src/str.c"
    "src/vec.c"
    "src/import/param_labels_csv.c"
    "src/import/reframed.c"
    "src/import/reframed_framedata.c"
    "src/import/reframed_framedata_1_5.c"
    "src/import/reframed_mapping_info.c"
    "src/import/reframed_metadata.c"
    "src/import/reframed_metadata_1_5.c"
    "src/import/reframed_metadata_1_6.c"
    "src/import/reframed_metadata_1_7.c"
    "src/import/reframed_motion_labels.c"
    "src/import/reframed_player_details.c"
    "src/import/reframed_replay.c"
    "src/import/reframed_videometadata.c"
    "src/import/reframed_videometadata_1_0.c"

    $<$<PLATFORM_ID:Linux>:$<$<BOOL:${VH_MEM_BACKTRACE}>:src/linux/backtrace_linux.c>>
    $<$<PLATFORM_ID:Linux>:src/linux/dynlib_linux.c>
    $<$<PLATFORM_ID:Linux>:src/linux/fs_linux.c>
    $<$<PLATFORM_ID:Linux>:src/linux/mfile_linux.c>
    $<$<PLATFORM_ID:Linux>:src/linux/thread_linux.c>
    $<$<PLATFORM_ID:Linux>:src/linux/utf8_linux.c>

    $<$<PLATFORM_ID:Windows>:$<$<BOOL:${VH_MEM_BACKTRACE}>:src/win32/backtrace_win32.c>>
    $<$<PLATFORM_ID:Windows>:src/win32/dynlib_win32.c>
    $<$<PLATFORM_ID:Windows>:src/win32/fs_win32.c>
    $<$<PLATFORM_ID:Windows>:src/win32/mfile_win32.c>
    $<$<PLATFORM_ID:Windows>:src/win32/thread_win32.c>
    $<$<PLATFORM_ID:Windows>:src/win32/utf8_win32.c>)
target_include_directories (vh
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_options (vh
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /wd4706 /wd4305 /wd4244>
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -Wshadow -Wconversion -Wno-unused-parameter $<$<BOOL:${VH_PROFILING}>:-pg -fno-omit-frame-pointer>>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -Wshadow -Wconversion -Wno-unused-parameter $<$<BOOL:${VH_PROFILING}>:-pg -fno-omit-frame-pointer>>)
target_compile_definitions (vh
    PRIVATE
        VH_BUILDING
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
target_link_libraries (vh
    PRIVATE
        $<$<PLATFORM_ID:Linux>:${CMAKE_DL_LIBS}>
        $<$<PLATFORM_ID:Windows>:$<$<BOOL:${VH_MEM_BACKTRACE}>:Dbghelp>>
        $<$<PLATFORM_ID:Windows>:wsock32>
        $<$<PLATFORM_ID:Windows>:ws2_32>
        VODHound::json-c
        ZLIB::ZLIB
        sqlite)
set_target_properties (vh
    PROPERTIES
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
        DEBUG_POSTFIX "d"
        LIBRARY_OUTPUT_DIRECTORY ${VODHOUND_BUILD_LIBDIR}
        LIBRARY_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_LIBDIR}
        LIBRARY_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_LIBDIR}
        RUNTIME_OUTPUT_DIRECTORY ${VODHOUND_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_BINDIR})

add_library (VODHound::vh ALIAS vh)

###############################################################################
# Unit tests
###############################################################################

if (VH_TESTS)
    enable_language (CXX)
    add_executable (tests
        "src/tests/test_btree.cpp"
        "src/tests/test_btree_as_set.cpp"
        "src/tests/test_hashmap.cpp"
        "src/tests/test_vector.cpp"
        "src/tests/env_library_init.cpp"
        "src/tests/main.cpp")
    target_link_libraries (tests PRIVATE vh)
    target_link_libraries (tests PRIVATE gmock)
endif ()

###############################################################################
# Benchmarks
###############################################################################

if (VH_BENCHMARKS)
    enable_language (CXX)
    add_executable (benchmarks
        "src/benchmarks/bench_hashmap.cpp"
        "src/benchmarks/bench_std_unordered_map.cpp"
        "src/benchmarks/bench_vector.cpp"
        "src/benchmarks/bench_std_vector.cpp"
        "src/benchmarks/main.cpp")
    target_link_libraries (benchmarks PRIVATE vh)
    target_link_libraries (benchmarks PRIVATE benchmark)
    target_include_directories (benchmarks
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/thirdparty/benchmark/include>)
endif ()

###############################################################################
# Database migration scripts
###############################################################################

set (MIGRATION_FILES
    "1-schema.down.sql"
    "1-schema.up.sql"
    "mappingInfo.json")
    
set (MIGRATION_FILES_IN)
set (MIGRATION_FILES_OUT)
foreach (file ${MIGRATION_FILES})
    set (in "${PROJECT_SOURCE_DIR}/migrations/${file}")
    set (out "${VODHOUND_BUILD_BINDIR}/migrations/${file}")
    list (APPEND MIGRATION_FILES_IN ${in})
    list (APPEND MIGRATION_FILES_OUT ${out})
    add_custom_command (
        OUTPUT ${out}
        DEPENDS ${in}
        COMMAND ${CMAKE_COMMAND} -E copy ${in} ${out}
        COMMENT "Copying migration file ${file}")
endforeach ()
add_custom_target (vh_schema ALL DEPENDS ${MIGRATION_FILES_OUT})
add_dependencies (vh vh_schema)

target_sources (vh PRIVATE ${MIGRATION_FILES_IN})

###############################################################################
# Install targets
###############################################################################

install (
    TARGETS vh
    EXPORT VODHoundTargets
    INCLUDES DESTINATION ${VODHOUND_INSTALL_INCLUDEDIR}
    ARCHIVE DESTINATION ${VODHOUND_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${VODHOUND_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${VODHOUND_INSTALL_BINDIR})
install (
    DIRECTORY "include/vh"
    DESTINATION ${VODHOUND_INSTALL_INCLUDEDIR})
install (
    DIRECTORY "${PROJECT_BINARY_DIR}/include/vh"
    DESTINATION ${VODHOUND_INSTALL_INCLUDEDIR})
