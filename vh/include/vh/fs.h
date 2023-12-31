#pragma once

#include "vh/config.h"
#include "vh/str.h"

C_BEGIN

struct path
{
    struct str str;
};

VH_PUBLIC_API int
fs_init(void);

VH_PRIVATE_API void
fs_deinit(void);

static inline void
path_init(struct path* path)
{
    str_init(&path->str);
}

static inline void
path_deinit(struct path* path)
{
    str_deinit(&path->str);
}

static inline struct str_view
path_view(struct path path)
{
    return str_view(path.str);
}

static inline void
path_terminate(struct path* path)
{
    str_terminate(&path->str);
}

VH_PUBLIC_API int
path_set(struct path* path, struct str_view str);

VH_PUBLIC_API void
path_set_take(struct path* path, struct path* other);

static inline void
path_clear(struct path* path)
{
    str_clear(&path->str);
}

VH_PUBLIC_API int
path_join(struct path* path, struct str_view str);

VH_PUBLIC_API struct str_view
path_basename_view(const struct path* path);

VH_PUBLIC_API struct str_view
cpath_basename_view(const char* path);

static inline void
path_basename(struct path* path)
{
    struct str_view view = path_basename_view(path);
    memmove(path->str.data, view.data, (size_t)view.len);
    path->str.len = view.len;
}

VH_PUBLIC_API struct str_view
path_dirname_view(const struct path* path);

static inline void
path_dirname(struct path* path)
{
    path->str.len = path_dirname_view(path).len;
}

VH_PUBLIC_API int
fs_list(struct str_view path, int (*on_entry)(const char* name, void* user), void* user);

VH_PUBLIC_API int
fs_list_strlist(struct strlist* out, struct str_view path);

VH_PUBLIC_API int
fs_list_strlist_matching(
    struct strlist* out,
    struct str_view path,
    int (*match)(const char* str, void* user),
    void* user);

VH_PUBLIC_API int
fs_file_exists(const char* file_path);

VH_PUBLIC_API int
fs_dir_exists(const char* file_path);

VH_PUBLIC_API int
fs_make_dir(const char* path);

VH_PUBLIC_API int
fs_remove_file(const char* path);

VH_PUBLIC_API struct str_view
fs_appdata_dir(void);

C_END
