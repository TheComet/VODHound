find_path (GTK4_gtk_INCLUDE_DIRS
    NAMES "gtk/gtk.h"
    PATHS
        "${GTK4_ROOT}/include"
    PATH_SUFFIXES "gtk-4.0")
find_path (GTK4_glib_INCLUDE_DIR
    NAMES "glib.h"
    PATHS
        "${GTK4_ROOT}/include"
    PATH_SUFFIXES "glib-2.0")
find_path (GTK4_glibconfig_INCLUDE_DIR
    NAMES "glibconfig.h"
    PATHS
        "${GTK4_ROOT}/lib/glib-2.0/include")
set (GTK4_glib_INCLUDE_DIRS
    "${GTK4_glib_INCLUDE_DIR}"
    "${GTK4_glibconfig_INCLUDE_DIR}")
find_path (GTK4_cairo_INCLUDE_DIRS
    NAMES "cairo.h"
    PATHS
        "${GTK4_ROOT}/include/cairo")
find_path (GTK4_pango_INCLUDE_DIRS
    NAMES "pango/pango.h"
    PATHS
        "${GTK4_ROOT}/include"
    PATH_SUFFIXES "pango-1.0")
find_path (GTK4_harfbuzz_INCLUDE_DIRS
    NAMES "hb.h"
    PATHS
        "${GTK4_ROOT}/include/harfbuzz")
find_path (GTK4_gdk-pixbuf_INCLUDE_DIRS
    NAMES "gdk-pixbuf/gdk-pixbuf.h"
    PATHS
        "${GTK4_ROOT}/include"
    PATH_SUFFIXES "gdk-pixbuf-2.0")
find_path (GTK4_graphene_INCLUDE_DIR
    NAMES "graphene.h"
    PATHS
        "${GTK4_ROOT}/include/graphene-1.0")
find_path (GTK4_graphene-config_INCLUDE_DIR
    NAMES "graphene-config.h"
    PATHS
        "${GTK4_ROOT}/lib/graphene-1.0/include")
set (GTK4_graphene_INCLUDE_DIRS
    "${GTK4_graphene_INCLUDE_DIR}"
    "${GTK4_graphene-config_INCLUDE_DIR}")

find_library (GTK4_gtk_LIBRARY
    NAMES "gtk-4"
    PATHS
        "${GTK4_ROOT}/lib")
find_library (GTK4_glib_LIBRARY
    NAMES "glib-2.0"
    PATHS
        "${GTK4_ROOT}/lib")
find_library (GTK4_gobject_LIBRARY
    NAMES "gobject-2.0"
    PATHS
        "${GTK4_ROOT}/lib")
find_library (GTK4_gio_LIBRARY
    NAMES "gio-2.0"
    PATHS
        "${GTK4_ROOT}/lib")
set (GTK4_glib_deps
    "${GTK4_gobject_LIBRARY}"
    "${GTK4_gio_LIBRARY}")
find_library (GTK4_cairo_LIBRARY
    NAMES "cairo"
    PATHS
        "${GTK4_ROOT}/lib")
find_library (GTK4_pango_LIBRARY
    NAMES "pango-1.0"
    PATHS
        "${GTK4_ROOT}/lib")
find_library (GTK4_harfbuzz_LIBRARY
    NAMES "harfbuzz"
    PATHS
        "${GTK4_ROOT}/lib")
find_library (GTK4_gdk-pixbuf_LIBRARY
    NAMES "gdk_pixbuf-2.0"
    PATHS
        "${GTK4_ROOT}/lib")
find_library (GTK4_graphene_LIBRARY
    NAMES "graphene-1.0"
    PATHS
        "${GTK4_ROOT}/lib")

find_file (GTK4_gtk_RUNTIME
    NAMES "gtk-4-1.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_glib_RUNTIME
    NAMES "glib-2.0-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_gobject_RUNTIME
    NAMES "gobject-2.0-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_gio_RUNTIME
    NAMES "gio-2.0-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_gmodule_RUNTIME
    NAMES "gmodule-2.0-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_intl8_RUNTIME
    NAMES "intl-8.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_pangocairo_RUNTIME
    NAMES "pangocairo-1.0-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_fribidi_RUNTIME
    NAMES "fribidi-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_harfbuzz_RUNTIME
    NAMES "harfbuzz.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_cairo_RUNTIME
    NAMES "cairo-2.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_cairo-gobject_RUNTIME
    NAMES "cairo-gobject-2.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_pango_RUNTIME
    NAMES "pango-1.0-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_png_RUNTIME
    NAMES "png16-16.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_gdk-pixbuf_RUNTIME
    NAMES "gdk_pixbuf-2.0-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_epoxy_RUNTIME
    NAMES "epoxy-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_graphene_RUNTIME
    NAMES "graphene-1.0-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_pangowin32_RUNTIME
    NAMES "pangowin32-1.0-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_tiff_RUNTIME
    NAMES "tiff4.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_jpeg_RUNTIME
    NAMES "jpeg-8.2.2.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_cairo-script-interpreter_RUNTIME
    NAMES "cairo-script-interpreter-2.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_ffi_RUNTIME
    NAMES "ffi-7.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_z_RUNTIME
    NAMES "z.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_pcre_RUNTIME
    NAMES "pcre2-8-0.dll"
    PATHS
        "${GTK4_ROOT}/bin")
find_file (GTK4_freetype_RUNTIME
    NAMES "freetype-6.dll"
    PATHS
        "${GTK4_ROOT}/bin")
set (GTK4_RUNTIMES
    ${GTK4_gtk_RUNTIME}
    ${GTK4_glib_RUNTIME}
    ${GTK4_gobject_RUNTIME}
    ${GTK4_gio_RUNTIME}
    ${GTK4_gmodule_RUNTIME}
    ${GTK4_intl8_RUNTIME}
    ${GTK4_pangocairo_RUNTIME}
    ${GTK4_fribidi_RUNTIME}
    ${GTK4_harfbuzz_RUNTIME}
    ${GTK4_cairo_RUNTIME}
    ${GTK4_cairo-gobject_RUNTIME}
    ${GTK4_png_RUNTIME}
    ${GTK4_pango_RUNTIME}
    ${GTK4_gdk-pixbuf_RUNTIME}
    ${GTK4_epoxy_RUNTIME}
    ${GTK4_graphene_RUNTIME}
    ${GTK4_pangowin32_RUNTIME}
    ${GTK4_tiff_RUNTIME}
    ${GTK4_jpeg_RUNTIME}
    ${GTK4_cairo-script-interpreter_RUNTIME}
    ${GTK4_ffi_RUNTIME}
    ${GTK4_z_RUNTIME}
    ${GTK4_pcre_RUNTIME}
    ${GTK4_freetype_RUNTIME})
file (GLOB GTK4_RUNTIMES "${GTK4_ROOT}/bin/*.dll")
set (GTK4_RUNTIMES "${GTK4_RUNTIMES}" PARENT_SCOPE)

foreach (_component IN LISTS GTK4_FIND_COMPONENTS)
    if (GTK4_${_component}_INCLUDE_DIRS AND GTK4_${_component}_LIBRARY)
        if (NOT TARGET GTK4::${_component})
            add_library (GTK4::${_component} UNKNOWN IMPORTED)
            set_target_properties (GTK4::${_component} PROPERTIES
                IMPORTED_LOCATION ${GTK4_${_component}_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES "${GTK4_${_component}_INCLUDE_DIRS}"
                IMPORTED_LINK_INTERFACE_LIBRARIES "${GTK4_${_component}_deps}"
                IMPORTED_RUNTIME_ARTIFACTS "${GTK4_${_component}_RUNTIMES}")
        endif ()
        set (GTK4_${_component}_FOUND 1 PARENT_SCOPE)
    else ()
        message (FATAL_ERROR "Failed to find GTK4 component ${_component}\n"
            "lib: ${GTK4_${_component}_LIBRARY}\n"
            "include: ${GTK4_${_component}_INCLUDE_DIRS}")
    endif ()
endforeach ()
