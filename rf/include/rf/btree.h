#pragma once

#include "rf/config.h"
#include "rf/hash.h"

C_BEGIN

#if defined(RF_BTREE_64BIT_KEYS)
typedef uint64_t rf_btree_key;
#else
typedef uint32_t rf_btree_key;
#endif

#if defined(RF_BTREE_64BIT_CAPACITY)
typedef uint64_t rf_btree_size;
#else
typedef uint32_t rf_btree_size;
#endif

/* Memory address of the last valid key + 1 (i.e. dereferencing this is not a valid key) */
#define BTREE_KEY_END(btree) \
        BTREE_KEY(btree, rf_btree_count(btree))

/* Memory address of the first value assuming the specified capacity */
#define BTREE_VALUE_BEG_CAP(btree, cap) \
        (void*)((rf_btree_key*)(btree)->data + (cap))

/* Memory address of the first value */
#define BTREE_VALUE_BEG(btree) \
        BTREE_VALUE_BEG_CAP(btree, (btree)->capacity)

/* Memory address of the last valid value + 1 (i.e. dereferencing this is not a valid value) */
#define BTREE_VALUE_END(btree) \
        BTREE_VALUE(btree, rf_btree_count(btree))

/* Memory address of key at index i */
#define BTREE_KEY(btree, i) \
        ((rf_btree_key*)(btree)->data + (i))

/* Memory address of value at index i */
#define BTREE_VALUE(btree, i) \
        (void*)((uint8_t*)BTREE_VALUE_BEG(btree) + (btree)->value_size * (i))

#define BTREE_VALUE_CAP(btree, i, cap) \
        (void*)((uint8_t*)BTREE_VALUE_BEG_CAP(btree, cap) + (btree)->value_size * (i))

/* Convert a key memory address to an index */
#define BTREE_KEY_TO_IDX(btree, key) \
        (rf_btree_size)((rf_btree_key*)(key) - (rf_btree_key*)(btree)->data)

/* Convert a value memory address to an index */
#define BTREE_VALUE_TO_IDX(btree, value) \
        (rf_btree_size)(((uint8_t*)value - (uint8_t*)BTREE_VALUE_BEG(btree)) / rf_btree_value_size(btree))

#define BTREE_KV_SIZE(btree) \
        (sizeof(rf_btree_key) + (btree)->value_size)

#define BTREE_NEEDS_REALLOC(btree) \
        ((btree)->count == (btree)->capacity)

#define BTREE_INVALID_KEY ((rf_btree_key)-1)

/*!
 * @brief Implements a container of sorted key-value pairs stored in flattened
 * memory (sorted by key).
 */
struct rf_btree
{
    void* data;
    rf_btree_size count;
    rf_btree_size capacity;
    rf_btree_size value_size;
};

/*!
 * @brief Creates a new btree object on the heap.
 * @param[out] btree Pointer to the new object is written to this parameter.
 * @param[in] value_size The size in bytes of the values that will be stored.
 * You have the choice to either copy values in-place, in which case you can
 * specify sizeof(my_type_t), or, if you are working with very large value types
 * or with different-sized values (such as strings) you can specify sizeof(void*)
 * and store pointers to those objects instead. In the former case, you don't
 * have to worry about having to explicitly free() the data you store. In the
 * latter case, the btree only holds references to data but you are responsible
 * for managing the lifetime of each value.
 * @return Returns the newly created btree object. It must be freed with
 * btree_free() when no longer required.
 */
RF_PUBLIC_API struct rf_btree*
rf_btree_create(rf_btree_size value_size);

/*!
 * @brief Initialises an existing btree object.
 * @note This does **not** free existing items if they aren't in-place. If
 * you have inserted pointers to objects into your btree and call this, those
 * items will be lost and a memory leak will have been created.
 * @param[in] btree The btree object to initialise.
 */
RF_PUBLIC_API void
rf_btree_init(struct rf_btree* btree, rf_btree_size value_size);

RF_PUBLIC_API void
rf_btree_deinit(struct rf_btree* btree);

/*!
 * @brief Destroys an existing btree object and FREEs the underlying memory.
 * @note Elements inserted into the btree are not FREEd.
 * @param[in] btree The btree object to free.
 */
RF_PUBLIC_API void
rf_btree_free(struct rf_btree* btree);

/*!
 *
 */
RF_PUBLIC_API int
rf_btree_reserve(struct rf_btree* btree, rf_btree_size size);

/*!
 * @brief Inserts an item into the btree using a key.
 *
 * @note Complexity is O(log2(n)) to find the insertion point.
 *
 * @param[in] btree The btree object to insert into.
 * @param[in] key A unique key to assign to the item being inserted. The
 * key must not exist in the btree, or the item will not be inserted.
 * @param[in] value A pointer to the data to insert into the tree. The data
 * pointed to is copied into the structure. If you are storing pointers to
 * strings (having specified sizeof(char*) for value_size), you would pass
 * a double-pointer to the string for the pointer to be copied into the btree.
 * @return Returns 1 if insertion was successful. Returns 0 if the key already
 * exists (in which case nothing is inserted). Returns -1 if not enough memory
 * was available in the case of a reallocation.
 */
RF_PUBLIC_API int
rf_btree_insert_new(struct rf_btree* btree, rf_btree_key key, const void* value);

RF_PUBLIC_API void*
rf_btree_emplace_new(struct rf_btree* btree, rf_btree_key key);

/*!
 * @brief Updates an existing value. If the key doesn't exist, this function
 * does nothing.
 * @note This is a convenience function that uses btree_find() to obtain a
 * pointer to the value and memcpy'ing the new value into its place.
 * @param[in] btree A pointer to the btree object to change the value of.
 * @param[in] key The unique key associated with the value you want to change.
 * @param[in] value The new value to set.
 * @return Returns 1 if the value was found and updated. Returns 0 if the key
 * was not found. Nothing happens in this case.
 */
RF_PUBLIC_API int
rf_btree_set_existing(struct rf_btree* btree, rf_btree_key key, const void* value);

/*!
 * @brief If the key doesn't exist, inserts the new value and returns a pointer
 * to the inserted value. If the key does exist, nothing is inserted and a
 * pointer to the existing value is returned.
 * @param[in] btree The btree to insert into.
 * @param[in] key The unique key associated with the value you want to set.
 * @param[in] value A pointer to the data to insert into the tree. The data
 * must be at least btree_value_size() in bytes. This is set during btree
 * creation. If the item doesn't yet exist, the value is copied into the btree.
 * If the item does already exist, then this value is ignored.
 * @param[out] inserted_value Will be updated to point to either the newly
 * inserted value, or point to the existing value.
 * @return Returns 1 if the value already existed. Returns 0 if a new entry was
 * made. Return -1 if not enough memory was available in the case of a reallocation.
 */
RF_PUBLIC_API int
rf_btree_insert_or_get(struct rf_btree* btree, rf_btree_key key, const void* value, void** inserted_value);

RF_PUBLIC_API void*
rf_btree_emplace_or_get(struct rf_btree* btree, rf_btree_key key);

/*!
 * @brief Looks for the specified key in the btree and returns a pointer to the
 * value in the structure. This is useful if you need to store data directly in
 * the memory occupied by the pointer and wish to modify it.
 * @note Complexity is O(log2(n))
 * @warning The returned pointer can be invalidated if any insertions or deletions
 * are performed.
 * @param[in] btree The btree to search in.
 * @param[in] key The key to search for.
 */
RF_PUBLIC_API void*
rf_btree_find(const struct rf_btree* btree, rf_btree_key key);

RF_PUBLIC_API void*
rf_btree_find_prev(const struct rf_btree* btree, rf_btree_key key);

/*!
 * @brief Searches for a key that matches the specified value.
 * @note Complexity is O(n).
 * @param[in] btree The btree to search.
 * @param[in] value The value to search for.
 * @return Returns the key if it was successfully found, or BTREE_INVALID_KEY if
 * otherwise.
 */
RF_PUBLIC_API rf_btree_key*
rf_btree_find_key(const struct rf_btree* btree, const void* value);

/*!
 * @brief Searches for the value associated with the specified key and compares
 * the memory of that value with the memory pointed to by "value".
 * @param[in] btree The tree to search.
 * @param[in] key The key to search for.
 * @param[in] value A pointer to the value to compare. The pointed-to memory
 * block must be at least the size of btree_value_size().
 * @return Returns a logical "true" if the key was found and the values match.
 * Returns a logical "false" otherwise (i.e. 0)
 */
RF_PUBLIC_API int
rf_btree_find_and_compare(const struct rf_btree* btree, rf_btree_key key, const void* value);

/*!
 * @brief Gets any item from the btree.
 *
 * This is useful when you want to iterate and remove all items from the btree
 * at the same time.
 * @return Returns a pointer to a value in the tree. Which item is implementation
 * specific, but deterministic.
 */
RF_PUBLIC_API void*
rf_btree_get_any_value(const struct rf_btree* btree);

#define rf_btree_first_key(btree) (*BTREE_KEY(btree, 0))
#define rf_btree_last_key(btree) (*BTREE_KEY(btree, btree_count(btree) - 1))

/*!
 * @brief Returns 1 if the specified key exists, 0 if otherwise.
 * @param btree The btree to find the key in.
 * @param key The key to search for.
 * @return Returns a logical "true" if the key was found. Otherwise, returns
 * a logical "false" (i.e. 0).
 */
RF_PUBLIC_API int
rf_btree_key_exists(struct rf_btree* btree, rf_btree_key key);

/*!
 * @brief Erases an item from the btree matching the specified key.
 * @note Complexity is O(log2(n))
 * @param[in] btree The btree to erase from.
 * @param[in] key The key to search for.
 * @return Returns 1 if the key was found and erased successfully.
 * Returns 0 if the key was not found.
 */
RF_PUBLIC_API int
rf_btree_erase(struct rf_btree* btree, rf_btree_key key);

/*!
 * @brief Erases an item from the btree by value.
 * @note Complexity is O(n)
 * @param[in] btree The btree to erase from.
 * @param[in] value A pointer to a chunk of memory that is at least the size of
 * btree_value_size(btree) and contains the value to compare. The value size is
 * specified during btree creation.
 * @return Returns the key that was associated with the value, if found. Otherwise
 * returns BTREE_INVALID_KEY;
 */
RF_PUBLIC_API rf_btree_key
rf_btree_erase_value(struct rf_btree* btree, const void* value);

rf_btree_key
rf_btree_erase_index(struct rf_btree* btree, rf_btree_size idx);

/*!
 * @brief A variation of btree_erase_value() where the value parameter points
 * into the btree structure. Such a pointer can be obtained with e.g. btree_find().
 * This version is much faster because the value isn't searched for.
 * @note Complexity is O(1)
 * @param[in] btree The btree to erase from.
 * @param[in] value A pointer to a value stored inside the btree's internal
 * memory.
 * @return Returns the key that was associated with the value.
 */
RF_PUBLIC_API rf_btree_key
rf_btree_erase_internal_value(struct rf_btree* btree, const void* value);

/*!
 * @brief Erases all items in the tree, but keeps the underlying memory.
 * @note This does **not** free existing items if they aren't in-place. If
 * you have inserted pointers to objects into your btree and call this, those
 * items will be lost and a memory leak will have been created.
 * @param[in] btree The btree to clear.
 */
RF_PUBLIC_API void
rf_btree_clear(struct rf_btree* btree);

/*!
 * @brief Shrinks the underlying memory, usually via realloc(). If the btree
 * is empty, then the underlying memory will be freed.
 * @param[in] btree The tree to compact.
 */
RF_PUBLIC_API void
rf_btree_compact(struct rf_btree* btree);

/*!
 * @brief Returns the number of items in the specified btree.
 * @param[in] btree The btree to count the items of.
 */
#define rf_btree_count(btree) ((btree)->count)

/*!
 * @brief Returns the size of the value type being stored. The value size is
 * specified during btree creation.
 * @return The size of the value type being stored in bytes.
 */
#define rf_btree_value_size(btree) ((btree)->value_size)

/*!
 * @brief Returns the current capacity of the btree. This can be used to determine
 * when to call btree_compact(), for example.
 * @return Returns the number of items that would fit into the underlying buffer.
 * This value is always greater or equal to btree_count().
 */
#define rf_btree_capacity(btree)  ((btree)->capacity)

/*!
 * @brief Iterates over the specified btree's items and opens a FOR_EACH
 * scope.
 * @param[in] btree The btree to iterate.
 * @param[in] T The type of data being held in the btree.
 * @param[in] k The name to give the variable holding the current key. Will
 * be of type rf_btree_key.
 * @param[in] v The name to give the variable pointing to the current
 * item. Will be of type T*.
 */
#define RF_BTREE_FOR_EACH(btree, T, k, v) {                                   \
    rf_btree_size idx_##k;                                                    \
    rf_btree_key k;                                                           \
    T* v;                                                                     \
    assert(btree_value_size(btree) > 0);                                      \
    for(idx_##k = 0;                                                          \
        idx_##k != btree_count(btree) && (                                    \
            ((k = *BTREE_KEY(btree, idx_##k)) || 1) &&                        \
            (((v  = (T*)BTREE_VALUE(btree, idx_##k)) != NULL) || 1));         \
        ++idx_##k) {

/*!
 * @brief Iterates over the specified btree's keys and opens a FOR_EACH scope.
 */
#define RF_BTREE_KEYS_FOR_EACH(btree, k) {                                    \
    rf_btree_size idx_##k;                                                    \
    rf_btree_key k;                                                           \
    for(idx_##k = 0;                                                          \
        idx_##k != btree_count(btree) && ((k = *BTREE_KEY(btree, idx_##k)) || 1); \
        idx_##k++) {

/*!
 * @brief Closes a for each scope previously opened by BTREE_FOR_EACH.
 */
#define RF_BTREE_END_EACH }}

/*!
 * @brief Will erase the current item in a for loop from the btree. The current
 * key and value variables become invalid. However, you can use the "continue"
 * keyword to obtain the next valid key/value pair.
 * @param[in] btree A pointer to the btree object currently being iterated.
 * @param[in] k The name of the active "key" variable. This should be identical
 * to the parameter passed to BTREE_FOR_EACH or BTREE_KEYS_FOR_EACH.
 * as in BTREE_FOR_EACH.
 */
#define RF_BTREE_ERASE_CURRENT_ITEM_IN_FOR_LOOP(btree, k) do {                \
        btree_erase_index(btree, idx_##k);                                    \
        idx_##k--;                                                            \
    } while(0)

C_END
