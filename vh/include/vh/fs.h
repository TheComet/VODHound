#pragma once

#include "vh/config.h"
#include "vh/str.h"

C_BEGIN

struct path
{
    struct str str;
};

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

VH_PUBLIC_API struct path
path_take_str(struct str* str);

VH_PUBLIC_API int
path_join(struct path* path, struct str_view str);

VH_PUBLIC_API void
path_dirname(struct path* path);

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

VH_PUBLIC_API struct str_view
fs_appdata_dir(void);

C_END
