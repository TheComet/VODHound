#include "vh/db_ops.h"
#include "vh/mstream.h"

#include "json-c/json.h"

int
import_reframed_framedata_1_5(
        struct db_interface* dbi,
        struct db* db,
        struct mstream* ms, int game_id)
{
    int frame_count = (int)mstream_read_lu32(ms);
    int fighter_count = mstream_read_u8(ms);

    for (int fighter_idx = 0; fighter_idx != fighter_count; ++fighter_idx)
        for (int frame = 0; frame != frame_count; ++frame)
        {
            uint64_t timestamp = mstream_read_lu64(ms);
            uint32_t frames_left = mstream_read_lu32(ms);
            float posx = mstream_read_lf32(ms);
            float posy = mstream_read_lf32(ms);
            float damage = mstream_read_lf32(ms);
            float hitstun = mstream_read_lf32(ms);
            float shield = mstream_read_lf32(ms);
            uint16_t status = mstream_read_lu16(ms);
            uint32_t motion_l = mstream_read_lu32(ms);
            uint8_t motion_h = mstream_read_u8(ms);
            uint8_t hit_status = mstream_read_u8(ms);
            uint8_t stocks = mstream_read_u8(ms);
            uint8_t flags = mstream_read_u8(ms);

            uint64_t motion = ((uint64_t)motion_h << 32) | motion_l;
            int attack_connected = (flags & 0x01) ? 1 : 0;
            int facing_left = (flags & 0x02) ? 1 : 0;
            int opponent_in_hitlag = (flags & 0x04) ? 1 : 0;

            if (mstream_past_end(ms))
                return -1;

            if (dbi->frame.add(db, game_id, fighter_idx, timestamp, frame,
                    (int)frames_left, posx, posy, damage, hitstun, shield, status,
                    hit_status, motion, stocks, attack_connected, facing_left, opponent_in_hitlag) != 0)
                return -1;
        }

    if (!mstream_at_end(ms))
        return -1;

    return 0;
}
