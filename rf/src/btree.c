#include "rf/btree.h"
#include "rf/mem.h"
#include <assert.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
static int
btree_realloc(struct rf_btree* btree, rf_btree_size new_capacity)
{
    /* clamp to minimum configured capacity */
    if (new_capacity < RF_BTREE_MIN_CAPACITY)
        new_capacity = RF_BTREE_MIN_CAPACITY;

    /*
     * If btree hasn't allocated anything yet, just allocated the requested
     * amount of memory and return immediately.
     */
    if (!btree->data)
    {
        btree->data = rf_malloc(new_capacity * BTREE_KV_SIZE(btree));
        if (!btree->data)
            return -1;
        btree->capacity = new_capacity;
        return 0;
    }

    /*
     * If the new capacity is larger than the old capacity, then we realloc
     * before shifting around the data.
     */
    if (new_capacity >= btree->capacity)
    {
        void* new_data = rf_realloc(btree->data, new_capacity * BTREE_KV_SIZE(btree));
        if (!new_data)
            return -1;
        btree->data = new_data;
    }

    /*
     * The keys are correctly placed in memory, but now that the capacity has
     * grown, the values need to be moved forwards in the data buffer
     */
    /*if (insertion_index == BTREE_INVALID_KEY)*/
    {
        rf_btree_size old_capacity = btree->capacity;
        void* old_values = BTREE_VALUE_BEG_CAP(btree, old_capacity);
        void* new_values = BTREE_VALUE_BEG_CAP(btree, new_capacity);
        memmove(new_values, old_values, old_capacity * btree->value_size);
    }

    /*
     * If the new capacity is smaller than the old capacity, we have to realloc
     * after moving around the data as to not read from memory out of bounds
     * of the buffer.
     */
    if (new_capacity < btree->capacity)
    {
        void* new_data = rf_realloc(btree->data, new_capacity * BTREE_KV_SIZE(btree));
        if (new_data)
            btree->data = new_data;
        else
        {
            /*
             * This should really never happen, but if the realloc to a smaller
             * size fails, the btree will be in a consistent state if the
             * capacity is updated to the new capacity.
             */
        }
    }

    btree->capacity = new_capacity;

    return 0;
}

/* ------------------------------------------------------------------------- */
struct rf_btree*
rf_btree_create(rf_btree_size value_size)
{
    struct rf_btree* btree = rf_malloc(sizeof *btree);
    if (btree == NULL)
        return NULL;
    rf_btree_init(btree, value_size);
    return btree;
}

/* ------------------------------------------------------------------------- */
void
rf_btree_init(struct rf_btree* btree, rf_btree_size value_size)
{
    assert(btree);
    btree->data = NULL;
    btree->count = 0;
    btree->capacity = 0;
    btree->value_size = value_size;
}

/* ------------------------------------------------------------------------- */
void rf_btree_deinit(struct rf_btree* btree)
{
    assert(btree);
    rf_btree_clear(btree);
    rf_btree_compact(btree);
}

/* ------------------------------------------------------------------------- */
void
rf_btree_free(struct rf_btree* btree)
{
    assert(btree);
    rf_btree_deinit(btree);
    rf_free(btree);
}

/* ------------------------------------------------------------------------- */
int
rf_btree_reserve(struct rf_btree* btree, rf_btree_size size)
{
    if (btree->capacity < size)
        if (btree_realloc(btree, size) != 0)
            return -1;

    return 0;
}

/* ------------------------------------------------------------------------- */
/* algorithm taken from GNU GCC stdlibc++'s lower_bound function, line 2121 in stl_algo.h */
/* https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a02014.html */
/*
 * 1) If the key exists, then a pointer to that key is returned.
 * 2) If the key does not exist, then the first valid key who's value is less
 *    than the key being searched for is returned.
 * 3) If there is no key who's value is less than the searched-for key, the
 *    returned pointer will point to the address after the last valid key in
 *    the array.
 */
static rf_btree_key*
find_lower_bound(const struct rf_btree* btree, rf_btree_key key)
{
    rf_btree_size half;
    rf_btree_key* middle;
    rf_btree_key* found;
    rf_btree_size len;

    assert(btree);

    found = BTREE_KEY(btree, 0);  /* start search at key index 0 */
    len = rf_btree_count(btree);

    while (len > 0)
    {
        half = len >> 1;
        middle = found + half;
        if (*middle < key)
        {
            found = middle;
            ++found;
            len = len - half - 1;
        }
        else
            len = half;
    }

    return found;
}

/* ------------------------------------------------------------------------- */
int
rf_btree_insert_new(struct rf_btree* btree, rf_btree_key key, const void* value)
{
    rf_btree_key* lower_bound;
    rf_btree_size insertion_index;
    rf_btree_size entries_to_move;

    assert(btree);

    /* May need to realloc */
    if (BTREE_NEEDS_REALLOC(btree))
        if (btree_realloc(btree, btree->capacity * RF_BTREE_EXPAND_FACTOR) != 0)
            return -1;

    /* lookup location in btree to insert */
    lower_bound = find_lower_bound(btree, key);
    if (lower_bound < BTREE_KEY_END(btree) && *lower_bound == key)
        return 0;
    insertion_index = BTREE_KEY_TO_IDX(btree, lower_bound);

    /* Move entries out of the way to make space for new entry */
    entries_to_move = rf_btree_count(btree) - insertion_index;
    memmove(lower_bound + 1,
            lower_bound,
            entries_to_move * sizeof(rf_btree_key));
    memmove(BTREE_VALUE(btree, insertion_index + 1),
            BTREE_VALUE(btree, insertion_index),
            entries_to_move * btree->value_size);

    /* Copy key/value into storage */
    memcpy(BTREE_KEY(btree, insertion_index), &key, sizeof(rf_btree_key));
    if (btree->value_size)
        memcpy(BTREE_VALUE(btree, insertion_index), value, btree->value_size);
    btree->count++;

    return 1;
}

/* ------------------------------------------------------------------------- */
void*
rf_btree_emplace_new(struct rf_btree* btree, rf_btree_key key)
{
    rf_btree_key* lower_bound;
    rf_btree_size insertion_index;
    rf_btree_size entries_to_move;

    assert(btree);

    /* May need to realloc */
    if (BTREE_NEEDS_REALLOC(btree))
        if (btree_realloc(btree, btree->capacity * RF_BTREE_EXPAND_FACTOR) != 0)
            return NULL;

    /* lookup location in btree to insert */
    lower_bound = find_lower_bound(btree, key);
    if (lower_bound < BTREE_KEY_END(btree) && *lower_bound == key)
        return NULL;
    insertion_index = BTREE_KEY_TO_IDX(btree, lower_bound);

    /* Move entries out of the way to make space for new entry */
    entries_to_move = rf_btree_count(btree) - insertion_index;
    memmove(lower_bound + 1,
            lower_bound,
            entries_to_move * sizeof(rf_btree_key));
    memmove(BTREE_VALUE(btree, insertion_index + 1),
            BTREE_VALUE(btree, insertion_index),
            entries_to_move * btree->value_size);

    /* Copy key into storage */
    memcpy(BTREE_KEY(btree, insertion_index), &key, sizeof(rf_btree_key));
    btree->count++;

    return BTREE_VALUE(btree, insertion_index);
}

/* ------------------------------------------------------------------------- */
int
rf_btree_set_existing(struct rf_btree* btree, rf_btree_key key, const void* value)
{
    void* found;
    assert(btree);
    assert(btree->value_size > 0);
    assert(value);

    if ((found = rf_btree_find(btree, key)) == NULL)
        return 0;

    memcpy(found, value, btree->value_size);
    return 1;
}

/* ------------------------------------------------------------------------- */
int
rf_btree_insert_or_get(struct rf_btree* btree, rf_btree_key key, const void* value, void** inserted_value)
{
    rf_btree_key* lower_bound;
    rf_btree_size insertion_index;
    rf_btree_size entries_to_move;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);
    assert(inserted_value);

    /* May need to realloc */
    if (BTREE_NEEDS_REALLOC(btree))
        if (btree_realloc(btree, btree->capacity * RF_BTREE_EXPAND_FACTOR) != 0)
            return -1;

    /* lookup location in btree to insert */
    lower_bound = find_lower_bound(btree, key);
    insertion_index = BTREE_KEY_TO_IDX(btree, lower_bound);
    if (lower_bound < BTREE_KEY_END(btree) && *lower_bound == key)
    {
        /*memcpy(BTREE_VALUE(btree, insertion_index), *value, btree->value_size);*/
        *inserted_value = BTREE_VALUE(btree, insertion_index);
        return 1;
    }

    /* Move entries out of the way to make space for new entry */
    entries_to_move = rf_btree_count(btree) - insertion_index;
    memmove(lower_bound + 1, lower_bound, entries_to_move * sizeof(rf_btree_key));
    memmove(BTREE_VALUE(btree, insertion_index + 1), BTREE_VALUE(btree, insertion_index), entries_to_move * btree->value_size);

    memcpy(BTREE_KEY(btree, insertion_index), &key, sizeof(rf_btree_key));
    memcpy(BTREE_VALUE(btree, insertion_index), value, btree->value_size);
    *inserted_value = BTREE_VALUE(btree, insertion_index);
    btree->count++;

    return 0;
}

/* ------------------------------------------------------------------------- */
void*
rf_btree_emplace_or_get(struct rf_btree* btree, rf_btree_key key)
{
    rf_btree_key* lower_bound;
    rf_btree_size insertion_index;
    rf_btree_size entries_to_move;

    assert(btree);
    assert(btree->value_size > 0);

    /* May need to realloc */
    if (BTREE_NEEDS_REALLOC(btree))
        if (btree_realloc(btree, btree->capacity * RF_BTREE_EXPAND_FACTOR) != 0)
            return NULL;

    /* lookup location in btree to insert */
    lower_bound = find_lower_bound(btree, key);
    insertion_index = BTREE_KEY_TO_IDX(btree, lower_bound);
    if (lower_bound < BTREE_KEY_END(btree) && *lower_bound == key)
        return BTREE_VALUE(btree, insertion_index);

    /* Move entries out of the way to make space for new entry */
    entries_to_move = rf_btree_count(btree) - insertion_index;
    memmove(lower_bound + 1, lower_bound, entries_to_move * sizeof(rf_btree_key));
    memmove(BTREE_VALUE(btree, insertion_index + 1), BTREE_VALUE(btree, insertion_index), entries_to_move * btree->value_size);

    memcpy(BTREE_KEY(btree, insertion_index), &key, sizeof(rf_btree_key));
    btree->count++;

    return BTREE_VALUE(btree, insertion_index);
}

/* ------------------------------------------------------------------------- */
void*
rf_btree_find(const struct rf_btree* btree, rf_btree_key key)
{
    rf_btree_key* lower_bound;
    rf_btree_size idx;

    assert(btree);
    assert(btree->value_size > 0);

    lower_bound = find_lower_bound(btree, key);
    if (lower_bound >= BTREE_KEY_END(btree) || *lower_bound != key)
        return NULL;

    idx = BTREE_KEY_TO_IDX(btree, lower_bound);
    return BTREE_VALUE(btree, idx);
}

/* ------------------------------------------------------------------------- */
void*
rf_btree_find_prev(const struct rf_btree* btree, rf_btree_key key)
{
    rf_btree_key* lower_bound;
    rf_btree_size idx;

    assert(btree);
    assert(btree->value_size > 0);

    lower_bound = find_lower_bound(btree, key - 1);
    if (lower_bound >= BTREE_KEY_END(btree))
        return NULL;

    idx = BTREE_KEY_TO_IDX(btree, lower_bound);
    return BTREE_VALUE(btree, idx);
}

/* ------------------------------------------------------------------------- */
static rf_btree_size
rf_btree_find_index_of_matching_value(const struct rf_btree* btree, const void* value)
{
    void* current_value;
    rf_btree_size i;

    for (i = 0, current_value = BTREE_VALUE_BEG(btree);
         i != rf_btree_count(btree);
         ++i, current_value = BTREE_VALUE(btree, i))
    {
        if (memcmp(current_value, value, btree->value_size) == 0)
            return i;
    }

    return (rf_btree_size)-1;
}

/* ------------------------------------------------------------------------- */
rf_btree_key*
rf_btree_find_key(const struct rf_btree* btree, const void* value)
{
    rf_btree_size i;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);

    if ((i = rf_btree_find_index_of_matching_value(btree, value)) == (rf_btree_size)-1)
        return NULL;

    return BTREE_KEY(btree, i);
}

/* ------------------------------------------------------------------------- */
int
rf_btree_find_and_compare(const struct rf_btree* btree, rf_btree_key key, const void* value)
{
    void* inserted_value;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);

    inserted_value = rf_btree_find(btree, key);
    if (inserted_value == NULL)
        return 0;

    return memcmp(inserted_value, value, btree->value_size) == 0;
}

/* ------------------------------------------------------------------------- */
void*
rf_btree_get_any_value(const struct rf_btree* btree)
{
    assert(btree);
    assert(btree->value_size > 0);

    if (rf_btree_count(btree) == 0)
        return NULL;
    return BTREE_VALUE(btree, 0);
}

/* ------------------------------------------------------------------------- */
int
rf_btree_key_exists(struct rf_btree* btree, rf_btree_key key)
{
    rf_btree_key* lower_bound;

    assert(btree);

    lower_bound = find_lower_bound(btree, key);
    if (lower_bound < BTREE_KEY_END(btree) && *lower_bound == key)
        return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
rf_btree_key
rf_btree_erase_index(struct rf_btree* btree, rf_btree_size idx)
{
    rf_btree_key* lower_bound;
    rf_btree_key key;
    rf_btree_size entries_to_move;

    lower_bound = BTREE_KEY(btree, idx);
    key = *lower_bound;
    entries_to_move = rf_btree_count(btree) - idx;
    memmove(lower_bound, lower_bound+1, entries_to_move * sizeof(rf_btree_key));
    memmove(BTREE_VALUE(btree, idx), BTREE_VALUE(btree, idx+1), entries_to_move * btree->value_size);
    btree->count--;

    return key;
}

/* ------------------------------------------------------------------------- */
int
rf_btree_erase(struct rf_btree* btree, rf_btree_key key)
{
    rf_btree_key* lower_bound;

    assert(btree);

    lower_bound = find_lower_bound(btree, key);
    if (lower_bound >= BTREE_KEY_END(btree) || *lower_bound != key)
        return 1;

    rf_btree_erase_index(btree, BTREE_KEY_TO_IDX(btree, lower_bound));

    return 0;
}

/* ------------------------------------------------------------------------- */
rf_btree_key
rf_btree_erase_value(struct rf_btree* btree, const void* value)
{
    rf_btree_size idx;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);

    idx = rf_btree_find_index_of_matching_value(btree, value);
    if (idx == (rf_btree_size)-1)
        return BTREE_INVALID_KEY;

    return rf_btree_erase_index(btree, idx);
}

/* ------------------------------------------------------------------------- */
rf_btree_key
rf_btree_erase_internal_value(struct rf_btree* btree, const void* value)
{
    rf_btree_size idx;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);
    assert(BTREE_VALUE_BEG(btree) <= value);
    assert(value < BTREE_VALUE_END(btree));

    idx = BTREE_VALUE_TO_IDX(btree, value);
    return rf_btree_erase_index(btree, idx);
}

/* ------------------------------------------------------------------------- */
void
rf_btree_clear(struct rf_btree* btree)
{
    assert(btree);
    btree->count = 0;
}

/* ------------------------------------------------------------------------- */
void
rf_btree_compact(struct rf_btree* btree)
{
    assert(btree);

    if (rf_btree_count(btree) == 0)
    {
        if (btree->data != NULL)
            rf_free(btree->data);
        btree->data = NULL;
        btree->capacity = 0;
    }
    else
    {
        btree_realloc(btree, rf_btree_count(btree));
    }
}
