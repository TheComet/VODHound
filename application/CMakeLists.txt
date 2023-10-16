project (application
    LANGUAGES C
    VERSION ${VODHOUND_VERSION})

add_executable (application
    "include/application/import.h"

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

    "src/main.c")
target_include_directories (application
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries (application
    PRIVATE
        VODHound::vh
        VODHound::json-c
        ZLIB::ZLIB
        IUP::iup
        IUP::gfx
        IUP::imglib)

set_target_properties (application PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
    RUNTIME_OUTPUT_DIRECTORY ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${VODHOUND_BUILD_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${VODHOUND_BUILD_BINDIR}
    OUTPUT_NAME "VODHound")

install (
    TARGETS application
    DESTINATION ${VODHOUND_INSTALL_BINDIR})
