list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

# Required so external plugins can make use of our build tree
configure_file ("cmake/modules/VODHoundPlugin.cmake" "${VODHOUND_BUILD_CMAKEDIR}/modules/VODHoundPlugin.cmake" COPYONLY)

install (
    FILES "cmake/modules/VODHoundPlugin.cmake"
    DESTINATION "${VODHOUND_BUILD_CMAKEDIR}/modules")

message (STATUS "adding ffmpeg-video")
add_subdirectory ("ffmpeg-video")
