/*!
 * @file rb.h
 * @author TheComet
 *
 * The ring buffer consists of a read and write index, and a chunk of memory:
 * ```c
 * struct rb_t {
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
#include "vh/rb.h"
#include "vh/mem.h"

#include <string.h>
#include <assert.h>

#define IS_POWER_OF_2(x) (((x) & ((x)-1)) == 0)

 /* -------------------------------------------------------------------- */
int
rb_realloc(struct rb* rb, rb_size new_size)
{
    void* new_buffer;
    assert(IS_POWER_OF_2(new_size));

    new_buffer = realloc(rb->buffer, new_size * rb->value_size);
    if (new_buffer == NULL)
        return -1;
    rb->buffer = new_buffer;

    /* Is the data wrapped? */
    if (rb->read > rb->write)
    {
        memmove(
            rb->buffer + rb->capacity * rb->value_size,
            rb->buffer,
            (size_t)rb->write * rb->value_size);
        rb->write += (rb_idx)rb->capacity;
    }

    rb->capacity = new_size;

    return 0;
}

/* -------------------------------------------------------------------- */
void
rb_init(struct rb* rb, rb_size value_size)
{
    rb->buffer = NULL;
    rb->read = 0;
    rb->write = 0;
    rb->capacity = 1;
    rb->value_size = value_size;
}

/* -------------------------------------------------------------------- */
void
rb_deinit(struct rb* rb)
{
    if (rb->buffer != NULL)
        free(rb->buffer);
}

/* -------------------------------------------------------------------- */
int
rb_put(struct rb* rb, const void* data)
{
    void* value = rb_emplace(rb);
    if (value == NULL)
        return -1;

    memcpy(value, data, rb->value_size);

    return 0;
}

/* -------------------------------------------------------------------- */
void*
rb_emplace(struct rb* rb)
{
    void* value;

    if (rb_is_full(rb))
        if (rb_realloc(rb, rb->capacity * 2) < 0)
            return NULL;

    rb_idx write = rb->write;
    value = rb->buffer + write * rb->value_size;
    rb->write = (write + 1) & ((rb_idx)rb->capacity - 1);

    return value;
}

/* -------------------------------------------------------------------- */
int
rb_putr(struct rb* rb, const void* data)
{
    void* value = rb_emplacer(rb);
    if (value == NULL)
        return -1;

    memcpy(value, data, rb->value_size);

    return 0;
}

/* -------------------------------------------------------------------- */
void*
rb_emplacer(struct rb* rb)
{
    if (rb_is_full(rb))
        if (rb_realloc(rb, rb->capacity * 2) < 0)
            return NULL;

    rb_idx read = (rb->read - 1) & ((rb_idx)rb->capacity - 1);
    rb->read = read;
    return rb->buffer + read * rb->value_size;
}

/* -------------------------------------------------------------------- */
void*
rb_take(struct rb* rb)
{
    rb_idx read;
    void* data;
    if (rb_is_empty(rb))
        return NULL;

    read = rb->read;
    data = rb->buffer + read * rb->value_size;
    rb->read = (read + 1) & ((rb_idx)rb->capacity - 1);
    return data;
}

/* -------------------------------------------------------------------- */
void*
rb_takew(struct rb* rb)
{
    rb_idx write;
    void* data;
    if (rb_is_empty(rb))
        return NULL;

    write = (rb->write - 1) & ((rb_idx)rb->capacity - 1);
    data = rb->buffer + write * rb->value_size;
    rb->write = write;
    return data;
}

/* -------------------------------------------------------------------- */
void*
rb_insert_emplace(struct rb* rb, rb_idx idx)
{
    rb_idx insert, src, dst;

    assert(idx >= 0 && idx <= (int)rb_count(rb));

    if (rb_is_full(rb))
        if (rb_realloc(rb, rb->capacity * 2) < 0)
            return NULL;

    insert = (rb->read + idx) & ((rb_idx)rb->capacity - 1);
    dst = rb->write;
    src = (rb->write - 1) & ((rb_idx)rb->capacity - 1);
    while (dst != insert)
    {
        memcpy(
            rb->buffer + dst * rb->value_size,
            rb->buffer + src * rb->value_size,
            rb->value_size);
        dst = src;
        src = (src - 1) & ((rb_idx)rb->capacity - 1);
    }

    rb->write = (rb->write + 1) & ((rb_idx)rb->capacity - 1);

    return rb->buffer + insert * rb->value_size;
}

/* -------------------------------------------------------------------- */
int
rb_insert(struct rb* rb, rb_idx idx, const void* data)
{
    void* value = rb_insert_emplace(rb, idx);
    if (value == NULL)
        return -1;

    memcpy(value, data, rb->value_size);

    return 0;
}

/* -------------------------------------------------------------------- */
void
rb_erase(struct rb* rb, rb_idx idx)
{
    rb_idx src, dst;

    assert(idx >= 0 && idx < (int)rb_count(rb));

    dst = (rb->read + idx) & ((rb_idx)rb->capacity - 1);
    src = (dst + 1) & ((rb_idx)rb->capacity - 1);
    while (src != rb->write)
    {
        memcpy(
            rb->buffer + dst * rb->value_size,
            rb->buffer + src * rb->value_size,
            rb->value_size);
        dst = src;
        src = (src + 1) & ((rb_idx)rb->capacity - 1);
    }

    rb->write = dst;
}
