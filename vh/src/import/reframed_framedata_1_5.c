#include "vh/db.h"
#include "vh/frame_data.h"
#include "vh/mstream.h"

#include "json-c/json.h"

#include <stdio.h>

int
import_reframed_framedata_1_5(struct mstream* ms, int game_id)
{
    struct frame_data fdata;
    int frame_count = (int)mstream_read_lu32(ms);
    int fighter_count = mstream_read_u8(ms);

    if (frame_data_alloc_structure(&fdata, fighter_count, frame_count) < 0)
        return -1;

    for (int fighter_idx = 0; fighter_idx != fighter_count; ++fighter_idx)
        for (int frame = 0; frame != frame_count; ++frame)
        {
            uint32_t motion_l;
            uint8_t motion_h;

            if (mstream_bytes_left(ms) < 8+4+4+4+4+4+4+2+4+1+1+1+1)
                goto fail;

            fdata.timestamp[fighter_idx][frame]   = mstream_read_lu64(ms);
            fdata.frames_left[fighter_idx][frame] = mstream_read_lu32(ms);
            fdata.posx[fighter_idx][frame]        = mstream_read_lf32(ms);
            fdata.posy[fighter_idx][frame]        = mstream_read_lf32(ms);
            fdata.damage[fighter_idx][frame]      = mstream_read_lf32(ms);
            fdata.hitstun[fighter_idx][frame]     = mstream_read_lf32(ms);
            fdata.shield[fighter_idx][frame]      = mstream_read_lf32(ms);
            fdata.status[fighter_idx][frame]      = mstream_read_lu16(ms);
            motion_l                              = mstream_read_lu32(ms);
            motion_h                              = mstream_read_u8(ms);
            fdata.motion[fighter_idx][frame]      = ((uint64_t)motion_h << 32) | motion_l;
            fdata.hit_status[fighter_idx][frame]  = mstream_read_u8(ms);
            fdata.stocks[fighter_idx][frame]      = mstream_read_u8(ms);
            fdata.flags[fighter_idx][frame]       = mstream_read_u8(ms);
        }

    if (!mstream_at_end(ms))
        goto fail;

    if (frame_data_save(&fdata, game_id) != 0)
        goto fail;
    frame_data_deinit(&fdata);

    return 0;

fail:
    frame_data_deinit(&fdata);
    return -1;
}
