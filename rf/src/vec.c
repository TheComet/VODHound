#include <string.h>
#include <assert.h>
#include "rf/vec.h"
#include "rf/mem.h"

#define VEC_INVALID_INDEX (rf_vec_idx)-1

#define VECTOR_NEEDS_REALLOC(x) \
        ((x)->count == (x)->capacity)

/* ----------------------------------------------------------------------------
 * Static functions
 * ------------------------------------------------------------------------- */

/*!
 * @brief Expands the underlying memory.
 *
 * This implementation will expand the memory by a factor of 2 each time this
 * is called. All elements are copied into the new section of memory.
 * @param[in] insertion_index Set to VEC_INVALID_INDEX if (no space should be
 * made for element insertion. Otherwise this parameter specifies the index of
 * the element to "evade" when re-allocating all other elements.
 * @param[in] new_count The number of elements to allocate memory for.
 * @note No checks are performed to make sure the target size is large enough.
 */
static int
rf_vec_realloc(struct rf_vec *vec,
              rf_vec_idx insertion_index,
              rf_vec_size new_count);

/* ----------------------------------------------------------------------------
 * Exported functions
 * ------------------------------------------------------------------------- */
struct rf_vec*
rf_vec_create(const rf_vec_size element_size)
{
    struct rf_vec* vec;
    if ((vec = rf_malloc(sizeof *vec)) == NULL)
        return NULL;
    rf_vec_init(vec, element_size);
    return vec;
}

/* ------------------------------------------------------------------------- */
void
rf_vec_init(struct rf_vec* vec, const rf_vec_size element_size)
{
    assert(vec);
    memset(vec, 0, sizeof *vec);
    vec->element_size = element_size;
}

/* ------------------------------------------------------------------------- */
void
rf_vec_deinit(struct rf_vec* vec)
{
    assert(vec);

    if (vec->data != NULL)
        vec->data = NULL;
}

/* ------------------------------------------------------------------------- */
void
rf_vec_free(struct rf_vec* vec)
{
    assert(vec);
    rf_vec_deinit(vec);
    rf_free(vec);
}

/* ------------------------------------------------------------------------- */
void
rf_vec_clear(struct rf_vec* vec)
{
    assert(vec);
    /*
     * No need to free or overwrite existing memory, just reset the counter
     * and let future insertions overwrite
     */
    vec->count = 0;
}

/* ------------------------------------------------------------------------- */
void
rf_vec_compact(struct rf_vec* vec)
{
    assert(vec);

    if (vec->count == 0)
    {
        if (vec->data != NULL)
            rf_free(vec->data);
        vec->data = NULL;
        vec->capacity = 0;
    }
    else
    {
        /* If this fails (realloc shouldn't fail when specifying a smaller size
         * but who knows) it doesn't really matter. The vec will be in an
         * unchanged state and functionally still be identical */
        rf_vec_realloc(vec, VEC_INVALID_INDEX, rf_vec_count(vec));
    }
}

/* ------------------------------------------------------------------------- */
void*
rf_vec_take(struct rf_vec* vec)
{
    void* data = vec->data;
    vec->data = NULL;
    vec->capacity = 0;
    vec->count = 0;
    return data;
}

/* ------------------------------------------------------------------------- */
void
rf_vec_clear_compact(struct rf_vec* vec)
{
    assert(vec);

    if (vec->data != NULL)
        rf_free(vec->data);
    vec->count = 0;
    vec->capacity = 0;
    vec->data = NULL;
}

/* ------------------------------------------------------------------------- */
int
rf_vec_reserve(struct rf_vec* vec, rf_vec_size size)
{
    assert(vec);

    if (vec->capacity < size)
    {
        if (rf_vec_realloc(vec, VEC_INVALID_INDEX, size) != 0)
            return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int
rf_vec_resize(struct rf_vec* vec, rf_vec_size size)
{
    assert(vec);

    if (rf_vec_reserve(vec, size) != 0)
        return -1;

    vec->count = size;

    return 0;
}

/* ------------------------------------------------------------------------- */
void*
rf_vec_emplace(struct rf_vec* vec)
{
    void* emplaced;
    assert(vec);

    if (VECTOR_NEEDS_REALLOC(vec))
        if (rf_vec_realloc(vec,
                           VEC_INVALID_INDEX,
                           rf_vec_count(vec) * RF_VEC_EXPAND_FACTOR) != 0)
            return NULL;

    emplaced = vec->data + (vec->element_size * vec->count);
    ++(vec->count);

    return emplaced;
}

/* ------------------------------------------------------------------------- */
int
rf_vec_push(struct rf_vec* vec, const void* data)
{
    void* emplaced;

    assert(vec);
    assert(data);

    if ((emplaced = rf_vec_emplace(vec)) == NULL)
        return -1;

    memcpy(emplaced, data, vec->element_size);

    return 0;
}

/* ------------------------------------------------------------------------- */
int
rf_vec_push_vec(struct rf_vec* vec, const struct rf_vec* src_vec)
{
    assert(vec);
    assert(src_vec);

    /* make sure element sizes are equal */
    if (vec->element_size != src_vec->element_size)
        return -2;

    /* make sure there's enough space in the target vec */
    if (vec->count + src_vec->count > vec->capacity)
        if (rf_vec_realloc(vec, VEC_INVALID_INDEX, vec->count + src_vec->count) != 0)
            return -1;

    /* copy data */
    memcpy(vec->data + (vec->count * vec->element_size),
           src_vec->data,
           src_vec->count * vec->element_size);
    vec->count += src_vec->count;

    return 0;
}

/* ------------------------------------------------------------------------- */
void*
rf_vec_pop(struct rf_vec* vec)
{
    assert(vec);

    if (!vec->count)
        return NULL;

    --(vec->count);
    return vec->data + (vec->element_size * vec->count);
}

/* ------------------------------------------------------------------------- */
void*
rf_vec_insert_emplace(struct rf_vec* vec, rf_vec_idx index)
{
    rf_vec_idx offset;

    assert(vec);

    /*
     * Normally the last valid index is (capacity-1), but in this case it's valid
     * because it's possible the user will want to insert at the very end of
     * the vec.
     */
    assert(index <= vec->count);

    /* re-allocate? */
    if (vec->count == vec->capacity)
    {
        if (rf_vec_realloc(vec,
                           index,
                           rf_vec_count(vec) * RF_VEC_EXPAND_FACTOR) != 0)
            return NULL;
    }
    else
    {
        /* shift all elements up by one to make space for insertion */
        rf_vec_size total_size = vec->count * vec->element_size;
        offset = vec->element_size * index;
        memmove(vec->data + offset + vec->element_size,
                vec->data + offset,
                total_size - (rf_vec_size)offset);
    }

    /* element is inserted */
    ++vec->count;

    /* return pointer to memory of new element */
    return (void*)(vec->data + index * vec->element_size);
}

/* ------------------------------------------------------------------------- */
int
rf_vec_insert(struct rf_vec* vec, rf_vec_idx index, const void* data)
{
    void* emplaced;

    assert(vec);
    assert(data);

    if ((emplaced = rf_vec_insert_emplace(vec, index)) == NULL)
        return -1;

    memcpy(emplaced, data, vec->element_size);

    return 0;
}

/* ------------------------------------------------------------------------- */
void
rf_vec_erase_index(struct rf_vec* vec, rf_vec_idx index)
{
    assert(vec);
    assert(index < vec->count);

    if (index == vec->count - 1)
        /* last element doesn't require memory shifting, just pop it */
        rf_vec_pop(vec);
    else
    {
        /* shift memory right after the specified element down by one element */
        rf_vec_idx offset = vec->element_size * index;                  /* offset to the element being erased in bytes */
        rf_vec_size total_size = vec->element_size * vec->count;     /* total current size in bytes */
        memmove(vec->data + offset,                                    /* target is to overwrite the element specified by index */
                vec->data + offset + vec->element_size,             /* copy beginning from one element ahead of element to be erased */
                total_size - (rf_vec_size)offset - vec->element_size);  /* copying number of elements after element to be erased */
        --vec->count;
    }
}

/* ------------------------------------------------------------------------- */
void
rf_vec_erase_element(struct rf_vec* vec, void* element)
{
    void* last_element;

    assert(vec);
    last_element = vec->data + (vec->count-1) * vec->element_size;
    assert(element);
    assert(element >= (void*)vec->data);
    assert(element <= (void*)last_element);

    if (element != (void*)last_element)
    {
        memmove(element,                         /* target is to overwrite the element */
                (uint8_t*)element + vec->element_size,  /* read everything from next element */
                (rf_vec_size)((uint8_t*)last_element - (uint8_t*)element));  /* ptr1 - ptr2 yields signed result, but last_element is always larger than element */
    }
    --vec->count;
}

/* ------------------------------------------------------------------------- */
rf_vec_idx
rf_vec_find(const struct rf_vec* vec, const void* element)
{
    rf_vec_idx i;
    for (i = 0; i != rf_vec_count(vec); ++i)
    {
        void* current_element = rf_vec_get(vec, i);
        if (memcmp(current_element, element, vec->element_size) == 0)
            return i;
    }

    return rf_vec_count(vec);
}

#define rf_vec_get_scratch_element(vec) \
    ((vec)->data + (vec)->capacity * (vec)->element_size)

/* ------------------------------------------------------------------------- */
void
rf_vec_reverse(struct rf_vec* vec)
{
    assert(vec);

    uint8_t* begin = vec->data;
    uint8_t* end = vec->data + (vec->count - 1) * vec->element_size;
    uint8_t* tmp = rf_vec_get_scratch_element(vec);

    while (begin < end)
    {
        memcpy(tmp, begin, vec->element_size);
        memcpy(begin, end, vec->element_size);
        memcpy(end, tmp, vec->element_size);

        begin += vec->element_size;
        end -= vec->element_size;
    }
}

/* ----------------------------------------------------------------------------
 * Static functions
 * ------------------------------------------------------------------------- */
static int
rf_vec_realloc(struct rf_vec *vec,
              rf_vec_idx insertion_index,
              rf_vec_size new_capacity)
{
    void* new_data;

    /*
     * If vec hasn't allocated anything yet, just allocated the requested
     * amount of memory and return immediately. Make space at end of buffer
     * for one scratch element, which is required for swapping elements in
     * rf_vec_reverse().
     */
    if (!vec->data)
    {
        new_capacity = (new_capacity == 0 ? RF_VEC_MIN_CAPACITY : new_capacity);
        vec->data = rf_malloc((new_capacity + 1) * vec->element_size);
        if (!vec->data)
            return -1;
        vec->capacity = new_capacity;
        return 0;
    }

    /* Realloc the data. Make sure to have space for the swap element at the end */
    if ((new_data = rf_realloc(vec->data, (new_capacity  + 1) * vec->element_size)) == NULL)
        return -1;
    vec->data = new_data;

    /* if no insertion index is required, copy all data to new memory */
    if (insertion_index != VEC_INVALID_INDEX)
    {
        void* old_upper_elements = vec->data + (insertion_index + 0) * vec->element_size;
        void* new_upper_elements = vec->data + (insertion_index + 1) * vec->element_size;
        rf_vec_size upper_element_count = (rf_vec_size)(vec->capacity - insertion_index);
        memmove(new_upper_elements, old_upper_elements, upper_element_count * vec->element_size);
    }

    vec->capacity = new_capacity;

    return 0;
}
