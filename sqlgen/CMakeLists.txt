cmake_minimum_required (VERSION 3.3)

project ("sqlgen"
    LANGUAGES C
    VERSION 0.0.1)

add_executable (sqlgen
    "src/main.c")

macro (sqlgen_target name)
    set (sqlgen_target_PARAM_OPTIONS)
    set (sqlgen_target_PARAM_ONE_VALUE_KEYWORDS
        INPUT
        OUTPUT_HEADER)
    set (sqlgen_target_PARAM_MULTI_VALUE_KEYWORDS
        BACKENDS)
    cmake_parse_arguments (
        sqlgen_target_arg
        "${sqlgen_target_PARAM_OPTIONS}"
        "${sqlgen_target_PARAM_ONE_VALUE_KEYWORDS}"
        "${sqlgen_target_PARAM_MULTI_VALUE_KEYWORDS}"
        ${ARGN})

    if (NOT "${sqlgen_target_arg_UNPARSED_ARGUMENTS}" STREQUAL "")
        message (FATAL_ERROR "sqlgen_target (<name> INPUT <input file> OUTPUT_HEADER <output file> BACKENDS <sqlite [...]>)")
    endif ()

    set (_input_file "${sqlgen_target_arg_INPUT}")
    set (_output_header "${sqlgen_target_arg_OUTPUT_HEADER}")
    if (NOT IS_ABSOLUTE "${_input_file}")
        set (_input_file "${CMAKE_CURRENT_SOURCE_DIR}/${_input_file}")
    endif ()
    if (NOT IS_ABSOLUTE "${_output_header}")
        set (_output_header "${CMAKE_CURRENT_BINARY_DIR}/${_output_header}")
    endif ()

    get_filename_component (_output_dir "${_input_file}" DIRECTORY)

    string (REPLACE ";" "," _backends ${sqlgen_target_arg_BACKENDS})

    add_custom_command (OUTPUT "${_output_header}" "${_output_files}"
        COMMAND sqlgen
        ARGS -i ${_input_file} -o ${_output_dir} -b ${_backends}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        MAIN_DEPENDENCY ${_input_file}
        DEPENDS sqlgen
        COMMENT "[sqlgen][${name}] Generating SQL bindings for backends: ${_backends}"
        VERBATIM)

    unset (_backends)
    unset (_output_dir)
    unset (_output_header)
    unset (_input_file)
endmacro ()
