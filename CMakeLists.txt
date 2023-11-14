cmake_minimum_required (VERSION 3.21)

# Force a debug build if not specified
if (NOT DEFINED CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build")
endif ()
message (STATUS "Build type: ${CMAKE_BUILD_TYPE}")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

project ("VODHound"
    LANGUAGES)

set (VODHOUND_VERSION "0.0.1")

set (VODHOUND_BUILD_BINDIR "${PROJECT_BINARY_DIR}/bin")
set (VODHOUND_BUILD_LIBDIR "${VODHOUND_BUILD_BINDIR}/lib")
set (VODHOUND_BUILD_CMAKEDIR "${VODHOUND_BUILD_BINDIR}/share/VODHound/cmake")
set (VODHOUND_BUILD_PLUGINDIR "${VODHOUND_BUILD_BINDIR}/share/VODHound/plugins")

set (VODHOUND_INSTALL_BINDIR ".")
set (VODHOUND_INSTALL_LIBDIR "lib")
set (VODHOUND_INSTALL_INCLUDEDIR "include")
set (VODHOUND_INSTALL_CMAKEDIR "share/VODHound/cmake")
set (VODHOUND_INSTALL_PLUGINDIR "share/VODHound/plugins")

set (VODHOUND_UI "GTK4" CACHE STRING "Which UI toolkit to use")
set_property (CACHE VODHOUND_UI PROPERTY STRINGS "GTK4;IUP;OFF")
option (VODHOUND_TESTS "Compile unit tests" ON)

find_package (ZLIB)
if (NOT ZLIB_FOUND AND NOT TARGET ZLIB::ZLIB)
    # Need to compile static libraries with position independent code on because
    # rfcommon is a shared library
    set (ZLIB_PIC ON CACHE INTERNAL "" FORCE)
    add_subdirectory ("thirdparty/zlib-1.2.11")
endif ()

if (VODHOUND_TESTS)
    set (INSTALL_GTEST OFF CACHE INTERNAL "")
    add_subdirectory ("thirdparty/googletest")
    set_target_properties (gtest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    set_target_properties (gmock PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    add_subdirectory ("tests")
endif ()

add_subdirectory ("thirdparty/sqlite-3.43.1")
add_subdirectory ("thirdparty/json-c")
add_subdirectory ("sqlgen")
add_subdirectory ("vh")

if (VODHOUND_UI MATCHES "IUP")
    # Configure IUP
    set (IUP_BUILD_BINDIR ${VODHOUND_BUILD_BINDIR} CACHE INTERNAL "")
    set (IUP_INSTALL_BINDIR ${VODHOUND_INSTALL_BINDIR} CACHE INTERNAL "")
    set (IUP_INSTALL_LIBDIR ${VODHOUND_INSTALL_LIBDIR} CACHE INTERNAL "")
    set (IUP_INSTALL_INCLUDEDIR ${VODHOUND_INSTALL_INCLUDEDIR} CACHE INTERNAL "")
    set (IUP_EXAMPLES OFF CACHE INTERNAL "")

    add_subdirectory ("thirdparty/iup-cmake")
    add_subdirectory ("iupgfx")
    add_subdirectory ("application")

    # "application" target should be the startup project in VS
    set_property(
        DIRECTORY "${PROJECT_SOURCE_DIR}"
        PROPERTY VS_STARTUP_PROJECT application)
elseif (VODHOUND_UI MATCHES "GTK4")
    add_subdirectory ("application-gtk4")
    
    # "application" target should be the startup project in VS
    set_property(
        DIRECTORY "${PROJECT_SOURCE_DIR}"
        PROPERTY VS_STARTUP_PROJECT application-gtk4)
endif ()

if (VODHOUND_UI)
    #add_subdirectory ("plugins")
endif ()

# All subprojects have added targets to this export
# group, so make sure to install it
install (
    EXPORT VODHoundTargets
    NAMESPACE VODHound::
    DESTINATION ${VODHOUND_INSTALL_CMAKEDIR})
export (
    EXPORT VODHoundTargets
    NAMESPACE VODHound::
    FILE ${VODHOUND_BUILD_CMAKEDIR}/VODHoundTargets.cmake)

configure_file ("cmake/modules/TestVisibilityMacros.cmake" "${VODHOUND_BUILD_CMAKEDIR}/modules/TestVisibilityMacros.cmake" COPYONLY)

install (
    DIRECTORY "cmake/modules/"
    DESTINATION ${VODHOUND_INSTALL_CMAKEDIR}/modules)

include(CMakePackageConfigHelpers)
configure_package_config_file("cmake/templates/VODHoundConfig.cmake.in"
    "${VODHOUND_BUILD_CMAKEDIR}/VODHoundConfig.cmake"
    INSTALL_DESTINATION "${VODHOUND_INSTALL_CMAKEDIR}")
install(
    FILES "${VODHOUND_BUILD_CMAKEDIR}/VODHoundConfig.cmake"
    DESTINATION ${VODHOUND_INSTALL_CMAKEDIR})
