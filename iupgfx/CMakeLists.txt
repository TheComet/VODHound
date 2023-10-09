cmake_minimum_required (VERSION 3.21)

project ("iupgfx"
    VERSION 0.0.1
    LANGUAGES C)

if (CMAKE_SYSTEM_NAME MATCHES Windows)
    set (DEFAULT_GFX "DX11")
    set (POSSIBLE_GFX "DX11;WGL")
elseif (CMAKE_SYSTEM_NAME MATCHES CYGWIN)
    set (DEFAULT_GFX "WGL")
    set (POSSIBLE_GFX "DX11;WGL")
elseif (CMAKE_SYSTEM_NAME MATCHES Linux)
    set (DEFAULT_GFX "GLX")
    set (POSSIBLE_GFX "GLX")
endif ()
set (IUP3D_GFX ${DEFAULT_GFX} CACHE STRING "Which graphics API to use for rendering")
set_property (CACHE IUP3D_GFX PROPERTY STRINGS ${POSSIBLE_GFX})

add_library (iupgfx ${IUP_LIB_TYPE}
    "include/iupgfx.h"
    "src/iup_gfxcanvas.c")
target_include_directories (iupgfx
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../thirdparty/iup-cmake/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_link_libraries (iupgfx
    PRIVATE
        IUP::iup
        VODHound::vh)
target_compile_definitions (iupgfx
    PRIVATE
        $<$<CONFIG:Debug>:DEBUG>)
set_target_properties (iupgfx
    PROPERTIES
        DEBUG_POSTFIX "d"
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
        RUNTIME_OUTPUT_DIRECTORY ${IUP_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${IUP_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${IUP_BUILD_BINDIR}
        LIBRARY_OUTPUT_DIRECTORY ${IUP_BUILD_LIBDIR}
        LIBRARY_OUTPUT_DIRECTORY_DEBUG ${IUP_BUILD_LIBDIR}
        LIBRARY_OUTPUT_DIRECTORY_RELEASE ${IUP_BUILD_LIBDIR}
        ARCHIVE_OUTPUT_DIRECTORY ${IUP_BUILD_LIBDIR}
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${IUP_BUILD_LIBDIR}
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${IUP_BUILD_LIBDIR})

if (IUP3D_GFX MATCHES "DX11")
    target_sources (${PROJECT_NAME} PRIVATE "src/iup_gfxcanvas_win32_dx11.c")
    target_link_libraries (${PROJECT_NAME}
        PRIVATE
            d3d11
            dxgi
            d3dcompiler
            dxguid)
elseif (IUP3D_GFX MATCHES "WGL")
    target_sources (${PROJECT_NAME} PRIVATE "src/iup_gfxcanvas_win32_gl.c")
elseif (IUP3D_GFX MATCHES "GLX")
    find_package (OpenGL REQUIRED)
    target_sources (${PROJECT_NAME} PRIVATE
        "src/iup_gfxcanvas_glx.c"
        "src/gl/shader.c"
        "src/gl/api.c")
    target_link_libraries (${PROJECT_NAME} PRIVATE OpenGL::GLX)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Windows" AND IUP_LIB_TYPE MATCHES "SHARED")
    target_sources (iupgfx PRIVATE "src/iupgfx.def")
endif ()

add_library (IUP::gfx ALIAS iupgfx)
