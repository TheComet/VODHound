cmake_minimum_required (VERSION 3.21)

project ("search"
    VERSION 0.0.1
    LANGUAGES C CXX)

include (VODHoundPlugin)

find_package (GTK4 REQUIRED COMPONENTS
    gtk glib cairo pango harfbuzz gdk-pixbuf graphene)

vodhound_add_plugin (${PROJECT_NAME}
    SOURCES
        "src/asm_x86_64.c"
        "src/ast.c"
        "src/ast_ops.c"
        "src/ast_post.c"
        "src/dfa.c"
        "src/search_index.c"
        "src/nfa.c"
        "src/parser.c"
        "src/plugin_search.c"
        "src/parser.y"
        "src/scanner.lex"
    HEADERS
        "include/${PROJECT_NAME}/asm.h"
        "include/${PROJECT_NAME}/ast.h"
        "include/${PROJECT_NAME}/ast_ops.h"
        "include/${PROJECT_NAME}/ast_post.h"
        "include/${PROJECT_NAME}/dfa.h"
        "include/${PROJECT_NAME}/search_index.h"
        "include/${PROJECT_NAME}/match.h"
        "include/${PROJECT_NAME}/nfa.h"
        "include/${PROJECT_NAME}/range.h"
        "include/${PROJECT_NAME}/parser.h"
        "include/${PROJECT_NAME}/state.h"
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
        GTK4::glib
        GTK4::cairo
        GTK4::pango
        GTK4::harfbuzz
        GTK4::gdk-pixbuf
        GTK4::graphene
        GTK4::gtk)

if (VODHOUND_${PLUGIN_NAME})
    option (${PLUGIN_NAME}_EXPORT_DOT "Export AST/NFA/DFA of each stage as DOT format, and export transition tables" ON)
    option (${PLUGIN_NAME}_COUNTER_EXAMPLES "Have BISON generate counter examples" OFF)

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
    target_compile_definitions (${PROJECT_NAME}
        PRIVATE
            $<$<BOOL:${PLUGIN_NAME}_EXPORT_DOT>:EXPORT_DOT>)

    if (VODHOUND_TESTS)
        target_sources (${PROJECT_NAME}-tests
            PRIVATE
                ${BISON_${PROJECT_NAME}-parser_OUTPUTS}
                ${FLEX_${PROJECT_NAME}-scanner_OUTPUTS})
        target_include_directories (${PROJECT_NAME}-tests
            PRIVATE
                $<BUILD_INTERFACE:$<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${HAVE_UNISTD_H}>>>:${PROJECT_SOURCE_DIR}/include/win32_unistd>>)
        target_compile_definitions (${PROJECT_NAME}-tests
            PRIVATE
                $<$<BOOL:${PLUGIN_NAME}_EXPORT_DOT>:EXPORT_DOT>
                $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
    endif ()

    ###########################################################################
    # Graph layout library
    ###########################################################################

    #set (COIN_PIC ON CACHE BOOL "" FORCE)
    #set (OGDF_PIC ON CACHE BOOL "" FORCE)
    #add_subdirectory ("thirdparty/ogdf.v2022.02")
    #target_link_libraries (${PROJECT_NAME}
    #    PRIVATE OGDF)
endif ()
