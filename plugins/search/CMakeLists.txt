cmake_minimum_required (VERSION 3.21)

project ("search"
    VERSION 0.0.1
    LANGUAGES C CXX)

include (VODHoundPlugin)
vodhound_add_plugin (${PROJECT_NAME}
    SOURCES
        "src/asm_x86_64.c"
        "src/ast.c"
        "src/dfa.c"
        "src/nfa.c"
        "src/parser.c"
        "src/plugin_search.c"
        "src/parser.y"
        "src/scanner.lex"
    HEADERS
        "include/${PROJECT_NAME}/asm.h"
        "include/${PROJECT_NAME}/ast.h"
        "include/${PROJECT_NAME}/dfa.h"
        "include/${PROJECT_NAME}/frame_data.h"
        "include/${PROJECT_NAME}/match.h"
        "include/${PROJECT_NAME}/nfa.h"
        "include/${PROJECT_NAME}/range.h"
        "include/${PROJECT_NAME}/parser.h"
        "include/${PROJECT_NAME}/symbol.h"
    INCLUDES
        "include"
        "${PROJECT_BINARY_DIR}/include"
    TESTS
        "tests/test_ast.cpp"
        "tests/test_eval.cpp"
        "tests/test_dfa.cpp"
        "tests/test_nfa.cpp"
    LIBS
        VODHound::vh
        IUP::iup)

if (VODHOUND_${PLUGIN_NAME})
    option (${PLUGIN_NAME}_COUNTER_EXAMPLES "Have BISON generate counter examples" OFF)
    option (${PLUGIN_NAME}_CLI "Build standalone command line interface for testing" OFF)

    ###########################################################################
    # Parsers
    ###########################################################################

    find_package (FLEX 2.6 REQUIRED)
    find_package (BISON 3.8 REQUIRED)

    # These may not exist
    file (MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/src")
    file (MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}")

    if (${PLUGIN_NAME}_COUNTER_EXAMPLES)
        set (BISON_COMPILE_FLAGS -Wcounterexamples)
    endif ()

    bison_target (${PROJECT_NAME}-parser
        "${PROJECT_SOURCE_DIR}/src/parser.y"
        "${PROJECT_BINARY_DIR}/src/parser.y.c"
        DEFINES_FILE "${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}/parser.y.h"
        COMPILE_FLAGS ${BISON_COMPILE_FLAGS})
    flex_target (${PROJECT_NAME}-scanner
        "${PROJECT_SOURCE_DIR}/src/scanner.lex"
        "${PROJECT_BINARY_DIR}/src/scanner.lex.c"
        DEFINES_FILE "${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}/scanner.lex.h")
    add_flex_bison_dependency (${PROJECT_NAME}-scanner ${PROJECT_NAME}-parser)

    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set_source_files_properties (${FLEX_${PROJECT_NAME}-scanner_OUTPUTS} PROPERTIES
            COMPILE_FLAGS "/wd4005")
    endif ()

    check_include_file_cxx ("unistd.h" HAVE_UNISTD_H)

    target_sources (${PROJECT_NAME}
        PRIVATE
            ${BISON_${PROJECT_NAME}-parser_OUTPUTS}
            ${FLEX_${PROJECT_NAME}-scanner_OUTPUTS})
    target_include_directories (${PROJECT_NAME}
        PRIVATE
            $<BUILD_INTERFACE:$<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${HAVE_UNISTD_H}>>>:${PROJECT_SOURCE_DIR}/include/win32_unistd>>)

    if (VODHOUND_TESTS)
        target_sources (${PROJECT_NAME}_tests
            PRIVATE
                ${BISON_${PROJECT_NAME}-parser_OUTPUTS}
                ${FLEX_${PROJECT_NAME}-scanner_OUTPUTS})
        target_include_directories (${PROJECT_NAME}_tests
            PRIVATE
                $<BUILD_INTERFACE:$<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${HAVE_UNISTD_H}>>>:${PROJECT_SOURCE_DIR}/include/win32_unistd>>)
    endif ()

    ###########################################################################
    # Graph layout library
    ###########################################################################

    #set (COIN_PIC ON CACHE BOOL "" FORCE)
    #set (OGDF_PIC ON CACHE BOOL "" FORCE)
    #add_subdirectory ("thirdparty/ogdf.v2022.02")
    #target_link_libraries (${PROJECT_NAME}
    #    PRIVATE OGDF)

    ###########################################################################
    # CLI
    ###########################################################################

    if (${PLUGIN_NAME}_CLI)
        add_executable (${PROJECT_NAME}_cli
            "src/asm_x86_64.c"
            "src/ast.c"
            "src/dfa.c"
            "src/match.c"
            "src/nfa.c"
            "src/parser.c"
            "src/parser.y"
            "src/scanner.lex"

            "include/${PROJECT_NAME}/asm.h"
            "include/${PROJECT_NAME}/ast.h"
            "include/${PROJECT_NAME}/dfa.h"
            "include/${PROJECT_NAME}/frame_data.h"
            "include/${PROJECT_NAME}/match.h"
            "include/${PROJECT_NAME}/nfa.h"
            "include/${PROJECT_NAME}/range.h"
            "include/${PROJECT_NAME}/parser.h"
            "include/${PROJECT_NAME}/symbol.h"

            ${BISON_${PROJECT_NAME}-parser_OUTPUTS}
            ${FLEX_${PROJECT_NAME}-scanner_OUTPUTS}

            "src/cli.c")
        target_include_directories (${PROJECT_NAME}_cli
            PRIVATE
                "include"
                "${PROJECT_BINARY_DIR}/include"
                $<BUILD_INTERFACE:$<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${HAVE_UNISTD_H}>>>:${PROJECT_SOURCE_DIR}/include/win32_unistd>>)
        target_link_libraries (${PROJECT_NAME}_cli
            PRIVATE
                ${PROJECT_NAME}
                VODHound::vh)
        set_target_properties (${PROJECT_NAME}_cli
            PROPERTIES
                VS_DEBUGGER_WORKING_DIRECTORY ${VODHOUND_BUILD_BINDIR}
                MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
                RUNTIME_OUTPUT_DIRECTORY ${VODHOUND_BUILD_BINDIR}
                RUNTIME_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_BINDIR}
                RUNTIME_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_BINDIR})
    endif ()
endif ()
