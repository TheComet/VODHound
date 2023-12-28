#pragma once

#include "vh/config.h"
#include "vh/mfile.h"
#include <stdint.h>
#include <stddef.h>

C_BEGIN

enum frame_data_flags
{
    FRAME_DATA_ATTACK_CONNECTED   = 0x01,
    FRAME_DATA_FACING_LEFT        = 0x02,
    FRAME_DATA_OPPONENT_IN_HITLAG = 0x04
};

struct frame_data
{
    uint64_t** timestamp;
    uint64_t** motion;
    uint32_t** frames_left;
    float** posx;
    float** posy;
    float** damage;
    float** hitstun;
    float** shield;
    uint16_t** status;
    uint8_t** hit_status;
    uint8_t** stocks;
    uint8_t** flags;

    int fighter_count;
    int frame_count;

    struct mfile file;
};

VH_PUBLIC_API int
frame_data_alloc_structure(struct frame_data* fdata, int fighter_count, int frame_count);

static inline void
frame_data_init(struct frame_data* fdata)
    { fdata->file.address = NULL; fdata->fighter_count = 0; }

VH_PUBLIC_API void
frame_data_deinit(struct frame_data* fdata);

static inline void
frame_data_clear(struct frame_data* fdata)
    { frame_data_deinit(fdata); frame_data_init(fdata); }

VH_PUBLIC_API int
frame_data_load(struct frame_data* fdata, int game_id);

VH_PUBLIC_API int
frame_data_save(const struct frame_data* fdata, int game_id);

VH_PUBLIC_API void
frame_data_delete(int game_id);

VH_PUBLIC_API void
frame_data_delete_all(void);

C_END
