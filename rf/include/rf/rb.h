/*!
 * @file rb.h
 * @author TheComet
 *
 * The ring buffer consists of a read and write index, and a chunk of memory:
 * ```c
 * struct rf_rb_t {
 *     int read, write;
 *     T data[N];
 * }
 * ```
 *
 * The buffer is considered empty when rb->read == rb->write. It is considered
 * full when rb->write is one slot behind rb->read. This is necessary because
 * otherwise there would be no way to tell the difference between an empty and
 * a full ring buffer.
 */
#pragma once

#include "rf/config.h"
#include <stdint.h>

C_BEGIN

/*
 * For the following 6 macros it is very important that each argument is only
 * accessed once.
 */

#define RB_COUNT_N(rb, N)                                               \
        (((rb)->write - (rb)->read) & ((N)-1u))

#define RB_SPACE_N(rb, N)                                               \
        (((rb)->read - (rb)->write - 1) & ((N)-1u))

#define RB_IS_FULL_N(rb, N)                                             \
        ((rf_rb_idx)(((rb)->write + 1u) & ((N)-1u)) == (rb)->read)

#define RB_IS_EMPTY_N(rb, N)                                            \
        ((rb)->read == (rb)->write)

#define RB_COUNT_TO_END_N(result, rb, N) {                              \
            rf_rb_idx end = (rf_rb_idx)((N) - (rb)->read);              \
            rf_rb_idx n = (rf_rb_idx)((rb)->write + end) & ((N)-1u);    \
            result = (rf_rb_idx)(n < end ? n : end);                    \
        }

#define RB_SPACE_TO_END_N(result, rb, N) {                              \
            rf_rb_idx end = (rf_rb_idx)(((N)-1u) - (rb)->write);        \
            rf_rb_idx n = (rf_rb_idx)(end + (rb)->read) & ((N)-1u);     \
            result = n <= end ? n : end + 1u;                           \
        }

typedef int16_t rf_rb_idx;
typedef uint16_t rf_rb_size;

struct rf_rb
{
    char* buffer;
    rf_rb_size value_size;
    rf_rb_size capacity;
    rf_rb_idx read, write;
};

RF_PUBLIC_API void
rf_rb_init(struct rf_rb* rb, rf_rb_size value_size);

RF_PUBLIC_API void
rf_rb_deinit(struct rf_rb* rb);

RF_PUBLIC_API int
rf_rb_realloc(struct rf_rb* rb, rf_rb_size new_size);

RF_PUBLIC_API int
rf_rb_put(struct rf_rb* rb, const void* data);

RF_PUBLIC_API void*
rf_rb_emplace(struct rf_rb* rb);

RF_PUBLIC_API int
rf_rb_putr(struct rf_rb* rb, const void* data);

RF_PUBLIC_API void*
rf_rb_emplacer(struct rf_rb* rb);

RF_PUBLIC_API void*
rf_rb_take(struct rf_rb* rb);

RF_PUBLIC_API void*
rf_rb_takew(struct rf_rb* rb);

RF_PUBLIC_API void*
rf_rb_insert_emplace(struct rf_rb* rb, rf_rb_idx idx);

RF_PUBLIC_API int
rf_rb_insert(struct rf_rb* rb, rf_rb_idx idx, const void* data);

RF_PUBLIC_API void
rf_rb_erase(struct rf_rb* rb, rf_rb_idx idx);

#define rf_rb_clear(rb) \
    (rb)->read = (rb)->write

#define rf_rb_peek_read(rb) \
    (void*)((rb)->buffer + (rb)->read * (rb)->value_size)

#define rf_rb_peek_write(rb) \
    (void*)((rb)->buffer + (((rb)->write - 1) & ((rb)->capacity - 1)) * (rb)->value_size)

#define rf_rb_peek(rb, idx) \
    (void*)((rb)->buffer + (((rb)->read + (idx)) & ((rb)->capacity - 1)) * (rb)->value_size)

#define rf_rb_count(rb) \
    RB_COUNT_N(rb, (rb)->capacity)

#define rf_rb_is_full(rb) \
    RB_IS_FULL_N(rb, (rb)->capacity)

#define rf_rb_is_empty(rb) \
    RB_IS_EMPTY_N(rb, (rb)->capacity)

#define RB_FOR_EACH(rb, var_type, var) {                                    \
    var_type* var;                                                          \
    rf_rb_idx var##_i;                                                      \
    for(var##_i = (rb)->read,                                               \
        var = (var_type*)((rb)->buffer + (rb)->read * (rb)->value_size);    \
        var##_i != (rb)->write;                                             \
        var##_i = (var##_i + 1) & ((rb)->capacity - 1),                     \
        var = (var_type*)((rb)->buffer + var##_i * (rb)->value_size)) {

#define RB_END_EACH }}

C_END
