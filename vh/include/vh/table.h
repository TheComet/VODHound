#pragma once

#include "vh/config.h"
#include "vh/mem.h"
#include <string.h>

C_BEGIN

struct table
{
    void* data;
    int rows;
    int cols;
    int element_size;
    int capacity;
};

static inline int
table_init(struct table* table, int rows, int cols, unsigned int element_size)
{
    table->rows = rows;
    table->cols = cols;
    table->element_size = element_size;
    table->capacity = rows * cols * element_size;
    table->data = mem_alloc(table->capacity);
    if (table->data == NULL)
        return -1;
    return 0;
}

static inline void
table_deinit(struct table* table)
{
    mem_free(table->data);
}

static inline int
table_add_row(struct table* table)
{
    int row_size = table->cols * table->element_size;
    int table_size = table->rows * table->cols * table->element_size;
    while (table_size + row_size > table->capacity)
    {
        void* new_data = mem_realloc(table->data, table_size * 2);
        if (new_data == NULL)
            return -1;
        table->data = new_data;
        table->capacity = table_size * 2;
    }

    table->rows++;

    return 0;
}

static inline void
table_remove_row(struct table* table, int row)
{
    int row_size = table->cols * table->element_size;
    int table_size = row_size * table->rows;
    int offset = (row * table->cols) * table->element_size;
    if (row < table->rows - 1)
    {
        memmove(
            (void*)((char*)table->data + offset),
            (const void*)((char*)table->data + offset + row_size),
            table_size - offset - row_size);
    }

    table->rows--;
}

static inline void*
table_get(const struct table* table, int row, int col)
{
    int offset = (row * table->cols + col) * table->element_size;
    return (void*)((char*)table->data + offset);
}

C_END
