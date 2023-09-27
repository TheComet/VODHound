cmake_minimum_required (VERSION 3.21)

# Force a release build if not specified
if (NOT DEFINED CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build")
endif ()
message (STATUS "Build type: ${CMAKE_BUILD_TYPE}")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

project ("ReFramed2"
    LANGUAGES)

set (REFRAMED_VERSION "0.0.1")

set (REFRAMED_BUILD_BINDIR "${PROJECT_BINARY_DIR}/bin")
set (REFRAMED_BUILD_LIBDIR "${REFRAMED_BUILD_BINDIR}/lib")

set (REFRAMED_INSTALL_BINDIR ".")
set (REFRAMED_INSTALL_LIBDIR "lib")
set (REFRAMED_INSTALL_INCLUDEDIR "include")

add_subdirectory ("thirdparty/sqlite-3.43.1")
add_subdirectory ("thirdparty/json-c")
#add_subdirectory ("thirdparty/iup-cmake")
add_subdirectory ("rf")
add_subdirectory ("application")

