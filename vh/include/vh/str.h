#pragma once

#include "vh/config.h"

#include <stdint.h>
#include <string.h>

C_BEGIN

typedef uint32_t strlist_size;
typedef int32_t strlist_idx;

struct str
{
    char* data;
    int len;
};

struct str_view
{
    const char* data;
    int len;
};

static inline void
str_init(struct str* str)
{
    str->data = NULL;
    str->len = 0;
}

VH_PUBLIC_API void
str_deinit(struct str* str);

static inline struct str_view
str_view(struct str str)
{
    struct str_view view = {
        str.data,
        str.len
    };
    return view;
}
static inline struct str_view
cstr_view(const char* str)
{
    struct str_view view = {
        str,
        (int)strlen(str)
    };
    return view;
}
static inline struct str_view
cstr_view2(const char* str, int len)
{
    struct str_view view = { str, len };
    return view;
}

VH_PUBLIC_API int
str_set(struct str* str, struct str_view view);

static inline int
cstr_set(struct str* str, const char* cstr)
{
    return str_set(str, cstr_view(cstr));
}

VH_PUBLIC_API int
str_append(struct str* str, struct str_view other);

static inline int
cstr_append(struct str* str, const char* other)
{
    return str_append(str, cstr_view(other));
}

static inline int
str_terminate(struct str* str)
{
    return str_append(str, cstr_view2("", 1));
}

static inline void
str_replace_char(struct str* str, char search, char replace)
{
    int i;
    for (i = 0; i != str->len; ++i)
        if (str->data[i] == search)
            str->data[i] = replace;
}

static inline int
str_ends_with(struct str_view str, struct str_view cmp)
{
    if (str.len < cmp.len)
        return 0;
    const char* off = str.data + str.len - cmp.len;
    return memcmp(off, cmp.data, cmp.len) == 0;
}
static inline int
cstr_ends_with(struct str_view str, const char* cmp)
{
    return str_ends_with(str, cstr_view(cmp));
}

static inline int
cstr_cmp(struct str_view str, const char* cstr)
{
    return memcmp(str.data, cstr, (size_t)str.len);
}

VH_PUBLIC_API int
str_hex_to_u64(struct str_view str, uint64_t* out);

struct strlist_view
{
    strlist_idx off;
    strlist_idx len;
};

struct strlist
{
    char* data;
    struct strlist_view* strs;
    strlist_size count;     /* Number of strings in list*/
    strlist_size m_used;
    strlist_size m_alloc;
};

static inline void
strlist_init(struct strlist* sl)
{
    sl->data = NULL;
    sl->strs = NULL;
    sl->count = 0;
    sl->m_used = 0;
    sl->m_alloc = 0;
}

VH_PUBLIC_API void
strlist_deinit(struct strlist* sl);

int
strlist_add(struct strlist* sl, struct str_view str);

#define strlist_count(sl) ((sl)->count)

static inline struct str_view
strlist_get(const struct strlist* sl, strlist_idx i)
{
    struct str_view view;
    strlist_idx off = sl->strs[-i].off;
    view.len = sl->strs[-i].len;
    view.data = &sl->data[off];
    return view;
}

C_END
