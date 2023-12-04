#include "vh/db.h"
#include "vh/mem.h"
#include "vh/mstream.h"

#include "json-c/json.h"
#include "zlib.h"

int
import_reframed_framedata_1_5(struct mstream* ms, int game_id);

int
import_reframed_framedata(struct mstream* ms, int game_id)
{
    uint8_t major = mstream_read_u8(ms);
    uint8_t minor = mstream_read_u8(ms);
    if (major == 1 && minor == 5)
    {
        uLongf uncompressed_size = mstream_read_lu32(ms);
        if (uncompressed_size == 0 || uncompressed_size > 128*1024*1024)
            return -1;

        void* uncompressed_data = mem_alloc((mem_size)uncompressed_size);
        if (uncompress(
            (uint8_t*)uncompressed_data, &uncompressed_size,
            (const uint8_t*)mstream_ptr(ms), (uLongf)mstream_bytes_left(ms)) != Z_OK)
        {
            mem_free(uncompressed_data);
            return -1;
        }

        struct mstream uncompressed_stream = mstream_from_memory(
                uncompressed_data, (int)uncompressed_size);
        int result = import_reframed_framedata_1_5(&uncompressed_stream, game_id);
        mem_free(uncompressed_data);
        return result;
    }

    return -1;
}
