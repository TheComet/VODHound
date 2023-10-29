list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

# Required so external plugins can make use of our build tree
configure_file ("cmake/modules/VODHoundPlugin.cmake" "${VODHOUND_BUILD_CMAKEDIR}/modules/VODHoundPlugin.cmake" COPYONLY)
configure_file ("cmake/templates/test_main.cpp.in" "${VODHOUND_BUILD_CMAKEDIR}/templates/test_main.cpp.in" COPYONLY)

set (PLUGIN_TEST_MAIN_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/cmake/templates/test_main.cpp.in")

install (
    FILES "cmake/modules/VODHoundPlugin.cmake"
    DESTINATION "${VODHOUND_BUILD_CMAKEDIR}/modules")

add_subdirectory ("search")
add_subdirectory ("video-ffmpeg")
add_subdirectory ("vod-review")
