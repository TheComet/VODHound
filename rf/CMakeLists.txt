project ("rf"
    VERSION ${REFRAMED_VERSION}
    LANGUAGES C)

###############################################################################
# Options and config
###############################################################################

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set (DEBUG_FEATURE ON)
endif ()

set (RF_LIB_TYPE "SHARED" CACHE STRING "Build as shared or static")
set (RF_BTREE_EXPAND_FACTOR "2" CACHE STRING "When reallocating btree memory, this is the factor with which the buffer grows")
set (RF_BTREE_MIN_CAPACITY "32" CACHE STRING "The smallest number of elements to reserve when initializing a btree")
option (RF_BTREE_64BIT_KEYS "Enable 64-bit keys for btrees instead of 32-bit keys" OFF)
option (RF_BTREE_64BIT_CAPACITY "Enable btrees to allow up to 2^64 entries instead of 2^32" OFF)
option (RF_HM_STATS "Track hashmap usage statistics. This will increase sizeof(struct rf_hm)!" ${DEBUG_FEATURE})
set (RF_HM_REHASH_AT_PERCENT "70" CACHE STRING "How full the hash table needs to be before triggering a rehash, in percent")
set (RF_HM_MIN_CAPACITY "128" CACHE STRING "Default table size when creating new hashmaps")
set (RF_HM_EXPAND_FACTOR "4" CACHE STRING "When reallocating hashmap memory, thiis is the factor with which the buffer grows. WARNING: MUST be a power of 2!")
option (RF_MEM_BACKTRACE "Enable generating backtraces to every malloc/realloc call, making it easy to find where memory leaks occur" ${DEBUG_FEATURE})
set (RF_MEM_BACKTRACE_SIZE "64" CACHE STRING "Sets the maximum stack size (depth) when generating backtraces")
option (RF_MEM_DEBUGGING "Enable tracking malloc/realloc/free calls to detect memory leaks" ${DEBUG_FEATURE})
option (RF_PROFILING "Enable -pg and -fno-omit-frame-pointer" OFF)
option (RF_VEC_64BIT_CAPACITY "Set vector capacity to 2^64 instead of 2^32, but makes the structure 32 bytes instead of 20 bytes" OFF)
set (RF_VEC_EXPAND_FACTOR "2" CACHE STRING "When reallocating vector memory, this is the factor with which the buffer grows")
set (RF_VEC_MIN_CAPACITY "32" CACHE STRING "The smallest number of elements to reserve when initializing a vector")

# Memory debugging uses thread-local storage for its state because we don't want to deal with locks
include (CheckCSourceCompiles)
check_c_source_compiles ("__declspec(thread) int value; int main(void) { return 0; }" MSVC_THREADLOCAL)
check_c_source_compiles ("__thread int value; int main(void) { return 0; }" GCC_THREADLOCAL)
if (MSVC_THREADLOCAL)
    set (RF_THREADLOCAL "__declspec(thread)")
endif ()
if (GCC_THREADLOCAL)
    set (RF_THREADLOCAL "__thread")
endif ()

configure_file ("templates/config.h.in" "include/rf/config.h")

###############################################################################
# Library source files and settings
###############################################################################

add_library (rf ${RF_LIB_TYPE}
    "templates/config.h.in"
    "${PROJECT_BINARY_DIR}/include/rf/config.h"

    "src/btree.c"
    "src/hash.c"
    "src/hm.c"
    "src/init.c"
    "src/log.c"
    "src/mem.c"
    "src/mstream.c"
    "src/rb.c"
    "src/str.c"
    "src/vec.c"

    "include/rf/backtrace.h"
    "include/rf/btree.h"
    "include/rf/cli_colors.h"
    "include/rf/hash.h"
    "include/rf/hm.h"
    "include/rf/init.h"
    "include/rf/log.h"
    "include/rf/mem.h"
    "include/rf/mfile.h"
    "include/rf/mstream.h"
    "include/rf/rb.h"
    "include/rf/str.h"
    "include/rf/vec.h"

    $<$<PLATFORM_ID:Linux>:src/platform/linux/backtrace_linux.c>
    $<$<PLATFORM_ID:Linux>:src/platform/linux/mfile_linux.c>

    $<$<PLATFORM_ID:Windows>:src/platform/win32/backtrace_win32.c>)
target_include_directories (rf
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_options (rf
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /wd4706 /wd4305 /wd4244>
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -Wshadow -Wconversion -Wno-unused-parameter $<$<BOOL:${RF_PROFILING}>:-pg -fno-omit-frame-pointer>>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -Wshadow -Wconversion -Wno-unused-parameter $<$<BOOL:${RF_PROFILING}>:-pg -fno-omit-frame-pointer>>)
target_link_libraries (rf
    PRIVATE
        $<$<PLATFORM_ID:Windows>:$<$<BOOL:${RF_MEM_BACKTRACE}>:Dbghelp>>)

###############################################################################
# Unit tests
###############################################################################

if (RF_TESTS)
    enable_language (CXX)
    add_executable (rf_tests
        "src/tests/test_btree.cpp"
        "src/tests/test_btree_as_set.cpp"
        "src/tests/test_hashmap.cpp"
        "src/tests/test_vector.cpp"
        "src/tests/env_library_init.cpp"
        "src/tests/main.cpp")
    target_link_libraries (rf_tests PRIVATE rf)
    target_link_libraries (rf_tests PRIVATE gmock)
endif ()

###############################################################################
# Benchmarks
###############################################################################

if (RF_BENCHMARKS)
    enable_language (CXX)
    add_executable (rf_benchmarks
        "src/benchmarks/bench_hashmap.cpp"
        "src/benchmarks/bench_std_unordered_map.cpp"
        "src/benchmarks/bench_vector.cpp"
        "src/benchmarks/bench_std_vector.cpp"
        "src/benchmarks/main.cpp")
    target_link_libraries (rf_benchmarks PRIVATE rf)
    target_link_libraries (rf_benchmarks PRIVATE benchmark)
    target_include_directories (rf_benchmarks
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/thirdparty/benchmark/include>)
endif ()

