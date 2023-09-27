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

#include "rf/rb.h"
#include "rf/mem.h"

#include <string.h>
#include <assert.h>

#define IS_POWER_OF_2(x) (((x) & ((x)-1)) == 0)

 /* -------------------------------------------------------------------- */
int
rf_rb_realloc(struct rf_rb* rb, rf_rb_size new_size)
{
    void* new_buffer;
    assert(IS_POWER_OF_2(new_size));

    new_buffer = rf_realloc(rb->buffer, new_size * rb->value_size);
    if (new_buffer == NULL)
        return -1;
    rb->buffer = new_buffer;

    /* Is the data wrapped? */
    if (rb->read > rb->write)
    {
        memmove(
            rb->buffer + rb->capacity * rb->value_size,
            rb->buffer,
            (size_t)(rb->write * rb->value_size));
        rb->write += rb->capacity;
    }

    rb->capacity = new_size;

    return 0;
}

/* -------------------------------------------------------------------- */
void
rf_rb_init(struct rf_rb* rb, rf_rb_size value_size)
{
    rb->buffer = NULL;
    rb->read = 0;
    rb->write = 0;
    rb->capacity = 1;
    rb->value_size = value_size;
}

/* -------------------------------------------------------------------- */
void
rf_rb_deinit(struct rf_rb* rb)
{
    if (rb->buffer != NULL)
        rf_free(rb->buffer);
}

/* -------------------------------------------------------------------- */
int
rf_rb_put(struct rf_rb* rb, const void* data)
{
    void* value = rf_rb_emplace(rb);
    if (value == NULL)
        return -1;

    memcpy(value, data, rb->value_size);

    return 0;
}

/* -------------------------------------------------------------------- */
void*
rf_rb_emplace(struct rf_rb* rb)
{
    void* value;

    if (rf_rb_is_full(rb))
        if (rf_rb_realloc(rb, rb->capacity * 2) < 0)
            return NULL;

    rf_rb_idx write = rb->write;
    value = rb->buffer + write * rb->value_size;
    rb->write = (write + 1u) & (rb->capacity - 1u);

    return value;
}

/* -------------------------------------------------------------------- */
int
rf_rb_putr(struct rf_rb* rb, const void* data)
{
    void* value = rf_rb_emplacer(rb);
    if (value == NULL)
        return -1;

    memcpy(value, data, rb->value_size);

    return 0;
}

/* -------------------------------------------------------------------- */
void*
rf_rb_emplacer(struct rf_rb* rb)
{
    if (rf_rb_is_full(rb))
        if (rf_rb_realloc(rb, rb->capacity * 2) < 0)
            return NULL;

    rf_rb_idx read = (rb->read - 1u) & (rb->capacity - 1u);
    rb->read = read;
    return rb->buffer + read * rb->value_size;
}

/* -------------------------------------------------------------------- */
void*
rf_rb_take(struct rf_rb* rb)
{
    rf_rb_idx read;
    void* data;
    if (rf_rb_is_empty(rb))
        return NULL;

    read = rb->read;
    data = rb->buffer + read * rb->value_size;
    rb->read = (read + 1u) & (rb->capacity - 1u);
    return data;
}

/* -------------------------------------------------------------------- */
void*
rf_rb_takew(struct rf_rb* rb)
{
    rf_rb_idx write;
    void* data;
    if (rf_rb_is_empty(rb))
        return NULL;

    write = (rb->write - 1u) & (rb->capacity - 1u);
    data = rb->buffer + write * rb->value_size;
    rb->write = write;
    return data;
}

/* -------------------------------------------------------------------- */
void*
rf_rb_insert_emplace(struct rf_rb* rb, rf_rb_idx idx)
{
    rf_rb_idx insert, src, dst;

    assert(idx >= 0 && idx <= (int)rf_rb_count(rb));

    if (rf_rb_is_full(rb))
        if (rf_rb_realloc(rb, rb->capacity * 2) < 0)
            return NULL;

    insert = (rb->read + idx) & (rb->capacity - 1);
    dst = rb->write;
    src = (rb->write - 1u) & (rb->capacity - 1u);
    while (dst != insert)
    {
        memcpy(
            rb->buffer + dst * rb->value_size,
            rb->buffer + src * rb->value_size,
            rb->value_size);
        dst = src;
        src = (src - 1u) & (rb->capacity - 1u);
    }

    rb->write = (rb->write + 1u) & (rb->capacity - 1u);

    return rb->buffer + insert * rb->value_size;
}

/* -------------------------------------------------------------------- */
int
rf_rb_insert(struct rf_rb* rb, rf_rb_idx idx, const void* data)
{
    void* value = rf_rb_insert_emplace(rb, idx);
    if (value == NULL)
        return -1;

    memcpy(value, data, rb->value_size);

    return 0;
}

/* -------------------------------------------------------------------- */
void
rf_rb_erase(struct rf_rb* rb, rf_rb_idx idx)
{
    rf_rb_idx src, dst;

    assert(idx >= 0 && idx < (int)rf_rb_count(rb));

    dst = (rb->read + idx) & (rb->capacity - 1u);
    src = (dst + 1u) & (rb->capacity - 1u);
    while (src != rb->write)
    {
        memcpy(
            rb->buffer + dst * rb->value_size,
            rb->buffer + src * rb->value_size,
            rb->value_size);
        dst = src;
        src = (src + 1u) & (rb->capacity - 1u);
    }

    rb->write = dst;
}
