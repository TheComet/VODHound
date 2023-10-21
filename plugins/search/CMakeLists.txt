cmake_minimum_required (VERSION 3.21)

project ("search"
    VERSION 0.0.1
    LANGUAGES C CXX)

include (VODHoundPlugin)
vodhound_add_plugin (${PROJECT_NAME}
    SOURCES
        "src/ast.c"
        "src/nfa.c"
        "src/plugin_search.c"
        "src/parser.y"
        "src/scanner.lex"
    HEADERS
        "include/${PROJECT_NAME}/ast.h"
    INCLUDES
        "include"
    LIBS
        VODHound::vh
        IUP::iup)

if (VODHOUND_SEARCH)
    option (${PROJECT_NAME}_COUNTER_EXAMPLES "Have BISON generate counter examples" OFF)

    ###########################################################################
    # Parsers
    ###########################################################################

    find_package (FLEX 2.6 REQUIRED)
    find_package (BISON 3.8 REQUIRED)

    # These may not exist
    file (MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/src")
    file (MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}")

    if (${PROJECT_NAME}_COUNTER_EXAMPLES)
        set (BISON_COMPILE_FLAGS -Wcounterexamples)
    endif ()

    bison_target (${PROJECT_NAME}-query-parser
        "${PROJECT_SOURCE_DIR}/src/parser.y"
        "${PROJECT_BINARY_DIR}/src/parser.y.c"
        DEFINES_FILE "${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}/parser.y.h"
        COMPILE_FLAGS ${BISON_COMPILE_FLAGS})
    flex_target (${PROJECT_NAME}-query-scanner
        "${PROJECT_SOURCE_DIR}/src/scanner.lex"
        "${PROJECT_BINARY_DIR}/src/scanner.lex.c"
        DEFINES_FILE "${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}/scanner.lex.h")
    add_flex_bison_dependency (${PROJECT_NAME}-query-scanner ${PROJECT_NAME}-query-parser)

    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set_source_files_properties (${FLEX_${PROJECT_NAME}-query-scanner_OUTPUTS} PROPERTIES
            COMPILE_FLAGS "/wd4005")
    endif ()

    check_include_file_cxx ("unistd.h" HAVE_UNISTD_H)

    target_sources (${PROJECT_NAME}
        PRIVATE
            ${BISON_${PROJECT_NAME}-query-scanner_OUTPUTS}
            ${FLEX_${PROJECT_NAME}-query-scanner_OUTPUTS})
    target_include_directories (${PROJECT_NAME}
        PRIVATE
            $<BUILD_INTERFACE:$<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${HAVE_UNISTD_H}>>>:${PROJECT_SOURCE_DIR}/include/win32_unistd>>)

    ###########################################################################
    # Graph layout library
    ###########################################################################

    #set (COIN_PIC ON CACHE BOOL "" FORCE)
    #set (OGDF_PIC ON CACHE BOOL "" FORCE)
    #add_subdirectory ("thirdparty/ogdf.v2022.02")
    #target_link_libraries (${PROJECT_NAME}
    #    PRIVATE OGDF)
endif ()