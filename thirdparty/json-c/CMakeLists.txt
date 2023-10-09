# CMake 3.9 was released in 2017/07
#  As of 2023, many versions of Linux, NetBSD and FreeBSD provide,
#   and many OpenWRT packages require, much newer CMake packages.
#  We're stopping before 3.10 because that version starts requiring
#   c++11, which isn't available on e.g HPUX.
cmake_minimum_required(VERSION 3.21)

# JSON-C library is C only project.
# PROJECT_VERSION{,_MAJOR,_MINOR,_PATCH} set by project():
project(json-c
    LANGUAGES C
    VERSION 0.17.99)

# set default build type if not specified by user
if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Debug)
endif()

include(CheckSymbolExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckCSourceCompiles)
include(CheckTypeSize)

# Enable or disable features. By default, all features are turned off.
set (JSON_C_LIB_TYPE "STATIC" CACHE STRING  "Build as SHARED or STATIC library")
set_property (CACHE JSON_C_LIB_TYPE PROPERTY STRINGS "SHARED;STATIC")
option (JSON_C_DISABLE_BSYMBOLIC            "Avoid linking with -Bsymbolic-function."                OFF)
option (JSON_C_DISABLE_THREAD_LOCAL_STORAGE "Disable using Thread-Local Storage (HAVE___THREAD)."    OFF)
option (JSON_C_ENABLE_WERROR                "Avoid treating compiler warnings as fatal errors."      OFF)
option (JSON_C_ENABLE_RDRAND                "Enable RDRAND Hardware RNG Hash Seed."                  OFF)
option (JSON_C_ENABLE_THREADING             "Enable partial threading support."                      OFF)
option (JSON_C_OVERRIDE_GET_RANDOM_SEED     "Override json_c_get_random_seed() with custom code."    OFF)
option (JSON_C_DISABLE_EXTRA_LIBS           "Avoid linking against extra libraries, such as libbsd." ON)
option (JSON_C_DISABLE_JSON_POINTER         "Disable JSON pointer (RFC6901) and JSON patch support." OFF)
option (JSON_C_DISABLE_JSON_PATCH           "Disable JSON patch (RFC6902) support."                  OFF)
option (JSON_C_NEWLOCALE_NEEDS_FREELOCALE   "Work around newlocale bugs in old FreeBSD by calling freelocale"  OFF)
option (JSON_C_PIC                          "Compile with position independent code"                 OFF)
option (JSON_C_BUILD_APPS                   "Default to building apps" OFF)

check_include_file("fcntl.h"        HAVE_FCNTL_H)
check_include_file("inttypes.h"     HAVE_INTTYPES_H)
check_include_file(stdarg.h         HAVE_STDARG_H)
check_include_file(strings.h        HAVE_STRINGS_H)
check_include_file(string.h         HAVE_STRING_H)
check_include_file(syslog.h         HAVE_SYSLOG_H)


check_include_files("stdlib.h;stdarg.h;string.h;float.h" STDC_HEADERS)

check_include_file(unistd.h         HAVE_UNISTD_H)
check_include_file(sys/types.h      HAVE_SYS_TYPES_H)
check_include_file(sys/resource.h   HAVE_SYS_RESOURCE_H) # for getrusage

check_include_file("dlfcn.h"        HAVE_DLFCN_H)
check_include_file("endian.h"       HAVE_ENDIAN_H)
check_include_file("limits.h"       HAVE_LIMITS_H)
check_include_file("locale.h"       HAVE_LOCALE_H)
check_include_file("memory.h"       HAVE_MEMORY_H)

check_include_file(stdint.h         HAVE_STDINT_H)
check_include_file(stdlib.h         HAVE_STDLIB_H)
check_include_file(sys/cdefs.h      HAVE_SYS_CDEFS_H)
check_include_file(sys/param.h      HAVE_SYS_PARAM_H)
check_include_file(sys/random.h     HAVE_SYS_RANDOM_H)
check_include_file(sys/stat.h       HAVE_SYS_STAT_H)
check_include_file(xlocale.h        HAVE_XLOCALE_H)

# Set json-c specific vars to stamp into json_config.h
# in a way that hopefully won't conflict with other
# projects that use json-c.
if (HAVE_INTTYPES_H)
    set(JSON_C_HAVE_INTTYPES_H 1)
endif()
if (HAVE_STDINT_H)
    set(JSON_C_HAVE_STDINT_H 1)
endif()

check_symbol_exists(_isnan          "float.h" HAVE_DECL__ISNAN)
check_symbol_exists(_finite         "float.h" HAVE_DECL__FINITE)

if ((MSVC AND NOT (MSVC_VERSION LESS 1800)) OR MINGW OR CYGWIN OR UNIX)
    check_symbol_exists(INFINITY    "math.h" HAVE_DECL_INFINITY)
    check_symbol_exists(isinf       "math.h" HAVE_DECL_ISINF)
    check_symbol_exists(isnan       "math.h" HAVE_DECL_ISNAN)
    check_symbol_exists(NAN         "math.h" HAVE_DECL_NAN)
endif()

check_symbol_exists(_doprnt         "stdio.h" HAVE_DOPRNT)
if (UNIX OR MINGW OR CYGWIN)
    check_symbol_exists(snprintf    "stdio.h" HAVE_SNPRINTF)
endif()
check_symbol_exists(vasprintf       "stdio.h" HAVE_VASPRINTF)
check_symbol_exists(vsnprintf       "stdio.h" HAVE_VSNPRINTF)
check_symbol_exists(vprintf         "stdio.h" HAVE_VPRINTF)

check_symbol_exists(arc4random      "stdlib.h" HAVE_ARC4RANDOM)
if (NOT HAVE_ARC4RANDOM AND DISABLE_EXTRA_LIBS STREQUAL "OFF")
    check_include_file(bsd/stdlib.h HAVE_BSD_STDLIB_H)
    if (HAVE_BSD_STDLIB_H)
        list(APPEND CMAKE_REQUIRED_LIBRARIES "bsd")
        unset(HAVE_ARC4RANDOM CACHE)
        check_symbol_exists(arc4random   "bsd/stdlib.h" HAVE_ARC4RANDOM)
        if (NOT HAVE_ARC4RANDOM)
            list(REMOVE_ITEM CMAKE_REQUIRED_LIBRARIES "bsd")
        endif()
    endif()
endif()

if (HAVE_FCNTL_H)
    check_symbol_exists(open        "fcntl.h" HAVE_OPEN)
endif()
if (HAVE_STDLIB_H)
    check_symbol_exists(realloc     "stdlib.h" HAVE_REALLOC)
endif()
if (HAVE_LOCALE_H)
    check_symbol_exists(setlocale   "locale.h" HAVE_SETLOCALE)
    check_symbol_exists(uselocale   "locale.h" HAVE_USELOCALE)
endif()

# uClibc *intentionally* crashes in duplocale(), at least as of:
# https://github.com/ffainelli/uClibc/blob/266bdc1/libc/misc/locale/locale.c#L1322
# So, if it looks like we're compiling for a system like that just disable
# locale handling entirely.
exec_program(${CMAKE_C_COMPILER} ARGS -dumpmachine OUTPUT_VARIABLE CMAKE_GNU_C_MACHINE)
if (CMAKE_GNU_C_MACHINE MATCHES "uclibc")
    message(STATUS "Detected uClibc compiler, disabling locale handling")
    set(HAVE_SETLOCALE 0)
    set(HAVE_USELOCALE 0)
endif()

if (HAVE_STRINGS_H)
    check_symbol_exists(strcasecmp  "strings.h" HAVE_STRCASECMP)
    check_symbol_exists(strncasecmp "strings.h" HAVE_STRNCASECMP)
endif()
if (HAVE_STRING_H)
    check_symbol_exists(strdup      "string.h" HAVE_STRDUP)
    check_symbol_exists(strerror    "string.h" HAVE_STRERROR)
endif()
if (HAVE_SYSLOG_H)
    check_symbol_exists(vsyslog     "syslog.h" HAVE_VSYSLOG)
endif()
if (HAVE_SYS_RANDOM_H)
    check_symbol_exists(getrandom   "sys/random.h" HAVE_GETRANDOM)
endif()
if (HAVE_SYS_RESOURCE_H)
    check_symbol_exists(getrusage   "sys/resource.h" HAVE_GETRUSAGE)
endif()

check_symbol_exists(strtoll     "stdlib.h" HAVE_STRTOLL)
check_symbol_exists(strtoull    "stdlib.h" HAVE_STRTOULL)

set(json_c_strtoll "strtoll")
if (NOT HAVE_STRTOLL)
    # Use _strtoi64 if strtoll is not available.
    check_symbol_exists(_strtoi64 "stdlib.h" __have_strtoi64)
    if (__have_strtoi64)
        #set(HAVE_STRTOLL 1)
        set(json_c_strtoll "_strtoi64")
    endif()
endif()

set(json_c_strtoull "strtoull")
if (NOT HAVE_STRTOULL)
    # Use _strtoui64 if strtoull is not available.
    check_symbol_exists(_strtoui64 "stdlib.h" __have_strtoui64)
    if (__have_strtoui64)
        #set(HAVE_STRTOULL 1)
        set(json_c_strtoull "_strtoui64")
    endif()
endif()

check_type_size(int                 SIZEOF_INT)
check_type_size(int64_t             SIZEOF_INT64_T)
check_type_size(long                SIZEOF_LONG)
check_type_size("long long"         SIZEOF_LONG_LONG)
check_type_size("size_t"            SIZEOF_SIZE_T)
if (MSVC)
    check_type_size("SSIZE_T"           SIZEOF_SSIZE_T)
else()
    check_type_size("ssize_t"           SIZEOF_SSIZE_T)
endif()

check_c_source_compiles(
    "
    extern void json_object_get();
    __asm__(\".section .gnu.json_object_get\\n\\t.ascii \\\"Please link against libjson-c instead of libjson\\\"\\n\\t.text\");
    int main(int c, char *v) { return 0;}
    "
    HAS_GNU_WARNING_LONG
)

check_c_source_compiles(
  "int main() { int i, x = 0; i = __sync_add_and_fetch(&x,1); return x; }"
  HAVE_ATOMIC_BUILTINS)

if (NOT DISABLE_THREAD_LOCAL_STORAGE)
  check_c_source_compiles(
    "__thread int x = 0; int main() { return 0; }"
    HAVE___THREAD)

  if (HAVE___THREAD)
      set(SPEC___THREAD __thread)
  elseif (MSVC)
      set(SPEC___THREAD __declspec(thread))
  endif()
endif()

# Hardware random number is not available on Windows? Says, config.h.win32. Best to preserve compatibility.
if (WIN32)
    set(ENABLE_RDRAND 0)
endif()

# Once we've done basic symbol/header searches let's add them in.
configure_file(${PROJECT_SOURCE_DIR}/cmake/config.h.in        ${PROJECT_BINARY_DIR}/json-c/config.h)
message(STATUS "Wrote ${PROJECT_BINARY_DIR}/config.h")
configure_file(${PROJECT_SOURCE_DIR}/cmake/json_config.h.in   ${PROJECT_BINARY_DIR}/json-c/json_config.h)
message(STATUS "Wrote ${PROJECT_BINARY_DIR}/json_config.h")

if (NOT ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC"))
    check_c_source_compiles(
        "
        /* uClibc toolchains without threading barf when _REENTRANT is defined */
        #define _REENTRANT 1
        #include <sys/types.h>
        int main (void)
        {
          return 0;
        }
        "
        REENTRANT_WORKS
    )

    # OSX Mach-O doesn't support linking with '-Bsymbolic-functions'.
    # Others may not support it, too.
    #list(APPEND CMAKE_REQUIRED_LIBRARIES "-Wl,-Bsymbolic-functions")
    #check_c_source_compiles(
    #    "
    #    int main (void)
    #    {
    #      return 0;
    #    }
    #    "
    #    BSYMBOLIC_WORKS
    #)
    #list(REMOVE_ITEM CMAKE_REQUIRED_LIBRARIES "-Wl,-Bsymbolic-functions")

    #file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/check-version-script.sym" "TEST { global: *; };")
    #list(APPEND CMAKE_REQUIRED_LIBRARIES "-Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/check-version-script.sym")
    #check_c_source_compiles(
    #    "
    #    int main (void)
    #    {
    #      return 0;
    #    }
    #    "
    #    VERSION_SCRIPT_WORKS
    #)
    #list(REMOVE_ITEM CMAKE_REQUIRED_LIBRARIES "-Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/check-version-script.sym")
endif()

set(JSON_C_PUBLIC_HEADERS
    # Note: config.h is _not_ included here
    ${PROJECT_BINARY_DIR}/json-c/json_config.h
    ${PROJECT_BINARY_DIR}/json-c/json.h

    ${PROJECT_SOURCE_DIR}/include/json-c/arraylist.h
    ${PROJECT_SOURCE_DIR}/include/json-c/debug.h
    ${PROJECT_SOURCE_DIR}/include/json-c/json_c_version.h
    ${PROJECT_SOURCE_DIR}/include/json-c/json_inttypes.h
    ${PROJECT_SOURCE_DIR}/include/json-c/json_object.h
    ${PROJECT_SOURCE_DIR}/include/json-c/json_object_iterator.h
    ${PROJECT_SOURCE_DIR}/include/json-c/json_tokener.h
    ${PROJECT_SOURCE_DIR}/include/json-c/json_types.h
    ${PROJECT_SOURCE_DIR}/include/json-c/json_util.h
    ${PROJECT_SOURCE_DIR}/include/json-c/json_visit.h
    ${PROJECT_SOURCE_DIR}/include/json-c/linkhash.h
    ${PROJECT_SOURCE_DIR}/include/json-c/printbuf.h
)

set(JSON_C_HEADERS
    ${PROJECT_SOURCE_DIR}/src/json_object_private.h
    ${PROJECT_SOURCE_DIR}/src/json_pointer_private.h
    ${PROJECT_SOURCE_DIR}/src/random_seed.h
    ${PROJECT_SOURCE_DIR}/src/strerror_override.h
    ${PROJECT_SOURCE_DIR}/src/math_compat.h
    ${PROJECT_SOURCE_DIR}/src/snprintf_compat.h
    ${PROJECT_SOURCE_DIR}/src/strdup_compat.h
    ${PROJECT_SOURCE_DIR}/src/vasprintf_compat.h
)

set(JSON_C_SOURCES
    ${PROJECT_SOURCE_DIR}/src/arraylist.c
    ${PROJECT_SOURCE_DIR}/src/debug.c
    ${PROJECT_SOURCE_DIR}/src/json_c_version.c
    ${PROJECT_SOURCE_DIR}/src/json_object.c
    ${PROJECT_SOURCE_DIR}/src/json_object_iterator.c
    ${PROJECT_SOURCE_DIR}/src/json_tokener.c
    ${PROJECT_SOURCE_DIR}/src/json_util.c
    ${PROJECT_SOURCE_DIR}/src/json_visit.c
    ${PROJECT_SOURCE_DIR}/src/linkhash.c
    ${PROJECT_SOURCE_DIR}/src/printbuf.c
    ${PROJECT_SOURCE_DIR}/src/random_seed.c
    ${PROJECT_SOURCE_DIR}/src/strerror_override.c
)

if (NOT JSON_C_DISABLE_JSON_POINTER)
    set(JSON_C_PUBLIC_HEADERS   ${JSON_C_PUBLIC_HEADERS}  ${PROJECT_SOURCE_DIR}/include/json-c/json_pointer.h)
    set(JSON_C_SOURCES          ${JSON_C_SOURCES}         ${PROJECT_SOURCE_DIR}/src/json_pointer.c)
    set(JSON_H_JSON_POINTER "#include \"json-c/json_pointer.h\"")

    if (NOT JSON_C_DISABLE_JSON_PATCH)
        set(JSON_C_PUBLIC_HEADERS   ${JSON_C_PUBLIC_HEADERS}  ${PROJECT_SOURCE_DIR}/include/json-c/json_patch.h)
        set(JSON_C_SOURCES          ${JSON_C_SOURCES}         ${PROJECT_SOURCE_DIR}/src/json_patch.c)
    set(JSON_H_JSON_PATCH "#include \"json-c/json_patch.h\"")
    endif()
else()
    set(JSON_H_JSON_POINTER "")
    set(JSON_H_JSON_PATCH "")
endif()

configure_file(json.h.cmakein ${PROJECT_BINARY_DIR}/json-c/json.h @ONLY)

add_library(${PROJECT_NAME} ${JSON_C_LIB_TYPE}
    ${JSON_C_SOURCES}
    ${JSON_C_HEADERS}
    ${JSON_C_PUBLIC_HEADERS})
target_include_directories (${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_compile_definitions (${PROJECT_NAME}
    PRIVATE
        $<$<BOOL:${REENTRANT_WORKS}>:_REENTRANT>
        $<$<STREQUAL:${JSON_C_LIB_TYPE},SHARED>:JSON_C_DLL>
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_DEPRECATE>
        $<$<C_COMPILER_ID:GCC>:_GNU_SOURCE>
        $<$<C_COMPILER_ID:Clang>:_GNU_SOURCE>
        $<$<C_COMPILER_ID:AppleClang>:_GNU_SOURCE>)
target_compile_options (${PROJECT_NAME}
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/wd4996 /wd4100 /wd4996 /wd4244 /wd4706 /wd4702 /wd4127 /wd4701>
        $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>>:
            $<$<BOOL:${JSON_C_ENABLE_WERROR}>:-Werror>
            $<$<NOT:$<PLATFORM_ID:Windows>>:-Wstrict-prototypes>
            -Wall -Wcast-qual -Wextra -Wwrite-strings -Wno-unused-parameter -Wno-error=deprecated-declarations
            -ffunction-sections -fdata-sections>
        $<$<C_COMPILER_ID:Clang>:-Wno-shorten-64-to-32>)
target_link_libraries (${PROJECT_NAME}
    PRIVATE
        $<$<PLATFORM_ID:Linux>:m>)
set_target_properties(${PROJECT_NAME}
    PROPERTIES
        VERSION 5.3.0
        SOVERSION 5
        DEBUG_POSTFIX "d"
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
if (NOT JSON_C_DISABLE_BSYMBOLIC AND BSYMBOLIC_WORKS)
    #target_compile_options (json-c PRIVATE "-Wl,-Bsymbolic-functions")
endif()
if (VERSION_SCRIPT_WORKS AND JSON_C_LIB_TYPE MATCHES "SHARED")
    #target_link_options(json-c PRIVATE "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/json-c.sym")
endif()

if ($ENV{VALGRIND})
    # Build so that valgrind doesn't complain about linkhash.c
    target_compile_definitions (json-c PRIVATE "VALGRIND=1")
endif()

add_library (VODHound::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

install(
    TARGETS ${PROJECT_NAME}
    EXPORT VODHoundTargets
    RUNTIME DESTINATION ${VODHOUND_INSTALL_BINDIR}
    LIBRARY DESTINATION ${VODHOUND_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${VODHOUND_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${VODHOUND_INSTALL_INCLUDEDIR} ${VODHOUND_INSTALL_INCLUDEDIR}/json-c)

install(
    FILES ${JSON_C_PUBLIC_HEADERS}
    DESTINATION ${VODHOUND_INSTALL_INCLUDEDIR}/json-c)

