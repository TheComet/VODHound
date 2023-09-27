#pragma once

#include "rf/config.h"
#include "rf/hash.h"

#define RF_HM_SLOT_UNUSED    0
#define RF_HM_SLOT_RIP       1
#define RF_HM_SLOT_INVALID   2

C_BEGIN

typedef uint32_t rf_hm_size;
typedef int32_t rf_hm_idx;

struct rf_hm
{
    rf_hm_size       table_count;
    rf_hm_size       key_size;
    rf_hm_size       value_size;
    rf_hm_size       slots_used;
    rf_hash32_func   hash;
    char*            storage;
#ifdef RF_HASHMAP_STATS
    struct {
        uintptr_t total_insertions;
        uintptr_t total_deletions;
        uintptr_t total_tombstones;
        uintptr_t total_tombstone_reuses;
        uintptr_t total_rehashes;
        uintptr_t total_insertion_probes;
        uintptr_t total_deletion_probes;
        uintptr_t max_slots_used;
        uintptr_t max_slots_tombstoned;
        uint32_t current_tombstone_count;
    } stats;
#endif
};

/*!
 * @brief Allocates and initializes a new hm.
 * @param[out] hm A pointer to the new hm is written to this parameter.
 * Example:
 * ```cpp
 * struct hm_t* hm;
 * if (hm_create(&hm, sizeof(key_t), sizeof(value_t)) != RF_OK)
 *     handle_error();
 * ```
 * @param[in] key_size Specifies how many bytes of the "key" parameter to hash
 * in the hm_insert() call. Due to performance reasons, all keys are
 * identical in size. If you wish to use strings for keys, then you need to
 * specify the maximum possible string length here, and make sure you never
 * use strings that are longer than that (hm_insert_key contains a safety
 * check in debug mode for this case).
 * @note This parameter must be larger than 0.
 * @param[in] value_size Specifies how many bytes long the value type is. When
 * calling hm_insert(), value_size number of bytes are copied from the
 * memory pointed to by value into the hm.
 * @note This parameter may be 0.
 * @return If successful, returns HM_OK. If allocation fails, HM_OOM is returned.
 */
RF_PRIVATE_API struct rf_hm*
rf_hm_create(rf_hm_size key_size, rf_hm_size value_size);

RF_PRIVATE_API struct rf_hm*
rf_hm_create_with_options(
        rf_hm_size key_size,
        rf_hm_size value_size,
        rf_hm_size table_count,
        rf_hash32_func hash_func);

/*!
 * @brief Initializes a new hm. See hm_create() for details on
 * parameters and return values.
 */
RF_PRIVATE_API int
rf_hm_init(struct rf_hm* hm, rf_hm_size key_size, rf_hm_size value_size);

RF_PRIVATE_API int
rf_hm_init_with_options(
        struct rf_hm* hm,
        rf_hm_size key_size,
        rf_hm_size value_size,
        rf_hm_size table_count,
        rf_hash32_func hash_func);

/*!
 * @brief Cleans up internal resources without freeing the hm object itself.
 */
RF_PRIVATE_API void
rf_hm_deinit(struct rf_hm* hm);

/*!
 * @brief Cleans up all resources and frees the hm.
 */
RF_PRIVATE_API void
rf_hm_free(struct rf_hm* hm);

/*!
 * @brief Inserts a key and value into the hm.
 * @note Complexity is generally O(1). Inserting may cause a rehash if the
 * table size exceeds HM_REHASH_AT_PERCENT.
 * @param[in] hm A pointer to a valid hm object.
 * @param[in] key A pointer to where the key is stored. key_size number of
 * bytes are hashed and copied into the hm from this location in
 * memory. @see hm_create() regarding key_size.
 * @param[in] value A pointer to where the value is stored. value_size number
 * of bytes are copied from this location in memory into the hm. If
 * value_size is 0, then nothing is copied.
 * @return If the key already exists, then nothing is copied into the hm
 * and HM_EXISTS is returned. If the key is successfully inserted, HM_OK
 * is returned. If insertion failed, HM_OOM is returned.
 */
RF_PRIVATE_API int
rf_hm_insert(
    struct rf_hm* hm,
    const void* key,
    const void* value);

RF_PRIVATE_API void*
rf_hm_emplace(
    struct rf_hm* hm,
    const void* key);

RF_PRIVATE_API void*
rf_hm_erase(struct rf_hm* hm,
              const void* key);

RF_PRIVATE_API void*
rf_hm_find(const struct rf_hm* hm, const void* key);

RF_PRIVATE_API int
rf_hm_exists(const struct rf_hm* hm, const void* key);

#define rf_hm_count(hm) ((hm)->slots_used)

#define RF_HASHMAP_FOR_EACH(hm, key_t, value_t, key, value) { \
    key_t* key; \
    value_t* value; \
    rf_hm_idx pos_##value; \
    for (pos_##value = 0; \
        pos_##value != (hm)->table_count && \
            ((key = (key_t*)((hm)->storage + sizeof(rf_hash32) * (hm)->table_count + (hm)->key_size * pos_##value) || 1) && \
            ((value = (value_t*)((hm)->storage + sizeof(rf_hash32) * (hm)->table_count + (hm)->key_size * pos_##value + (hm)->value_size * pos_##value)) || 1); \
        ++pos_##value) \
    { \
        rf_hash32 slot_##value = *(rf_hash32*)((uint8_t*)(hm)->storage + (sizeof(rf_hash32) + (hm)->key_size) * pos_##value); \
        if (slot_##value == RF_HM_SLOT_UNUSED || slot_##value == RF_HM_SLOT_RIP || slot_##value == RF_HM_SLOT_INVALID) \
            continue; \
        { \


#define RF_HASHMAP_END_EACH }}}

C_END
