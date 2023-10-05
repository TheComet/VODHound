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

VH_PUBLIC_API void
path_set_take(struct path* path, struct str str);

VH_PUBLIC_API int
path_join(struct path* path, struct str_view str);

VH_PUBLIC_API int
fs_list(struct strlist* out, struct str_view path);

VH_PUBLIC_API int
fs_list_matching(struct strlist* out, struct str_view path, int (*match)(struct str_view str, const void* param), const void* param);

C_END
