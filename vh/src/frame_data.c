#include "vh/frame_data.h"
#include "vh/mem.h"
#include "vh/mstream.h"
#include "vh/utf8.h"

#include <string.h>

static mem_size frame_size =
    sizeof(uint64_t) +
    sizeof(uint64_t) +
    sizeof(uint32_t) +
    sizeof(float) +
    sizeof(float) +
    sizeof(float) +
    sizeof(float) +
    sizeof(float) +
    sizeof(uint16_t) +
    sizeof(uint8_t) +
    sizeof(uint8_t) +
    sizeof(uint8_t);

static void*
align_64(void* addr)
{
    uintptr_t value = (uintptr_t)addr;
    if (!(value & 0xF))
        return addr;
    value = (value + 0x10) & ~0xFUL;
    return (void*)value;
}

static void
init_pointers(void* mem, struct frame_data* fdata, int fighter_count)
{
    fdata->timestamp      = (uint64_t**)mem + 0  * fighter_count;
    fdata->motion         = (uint64_t**)mem + 1  * fighter_count;
    fdata->frames_left    = (uint32_t**)mem + 2  * fighter_count;
    fdata->posx           = (float**)mem    + 3  * fighter_count;
    fdata->posy           = (float**)mem    + 4  * fighter_count;
    fdata->damage         = (float**)mem    + 5  * fighter_count;
    fdata->hitstun        = (float**)mem    + 6  * fighter_count;
    fdata->shield         = (float**)mem    + 7  * fighter_count;
    fdata->status         = (uint16_t**)mem + 8  * fighter_count;
    fdata->hit_status     = (uint8_t**)mem  + 9  * fighter_count;
    fdata->stocks         = (uint8_t**)mem  + 10 * fighter_count;
    fdata->flags          = (uint8_t**)mem  + 11 * fighter_count;
}

int
frame_data_alloc_structure(struct frame_data* fdata, int fighter_count, int frame_count)
{
    int f;
    mem_size fighter_size = frame_size * (mem_size)frame_count;
    mem_size data_size = frame_size * (mem_size)fighter_count * (mem_size)frame_count;
    mem_size ptr_size = sizeof(void*) * 12 * (mem_size)fighter_count;
    mem_size align_padding = 0x8U * (mem_size)fighter_count;

    void* mem = mem_alloc(data_size + ptr_size + align_padding);
    if (mem == NULL)
        return -1;

    init_pointers(mem, fdata, fighter_count);

    for (f = 0; f != fighter_count; ++f)
    {
        fdata->timestamp[f]   = (uint64_t*)align_64((char*)mem + ptr_size + (mem_size)f * fighter_size);
        fdata->motion[f]      = (uint64_t*)(fdata->timestamp[f]    + (mem_size)frame_count);
        fdata->frames_left[f] = (uint32_t*)(fdata->motion[f]       + (mem_size)frame_count);
        fdata->posx[f]        = (float*)   (fdata->frames_left[f]  + (mem_size)frame_count);
        fdata->posy[f]        = (float*)   (fdata->posx[f]         + (mem_size)frame_count);
        fdata->damage[f]      = (float*)   (fdata->posy[f]         + (mem_size)frame_count);
        fdata->hitstun[f]     = (float*)   (fdata->damage[f]       + (mem_size)frame_count);
        fdata->shield[f]      = (float*)   (fdata->hitstun[f]      + (mem_size)frame_count);
        fdata->status[f]      = (uint16_t*)(fdata->shield[f]       + (mem_size)frame_count);
        fdata->hit_status[f]  = (uint8_t*) (fdata->status[f]       + (mem_size)frame_count);
        fdata->stocks[f]      = (uint8_t*) (fdata->hit_status[f]   + (mem_size)frame_count);
        fdata->flags[f]       = (uint8_t*) (fdata->stocks[f]       + (mem_size)frame_count);
    }

    fdata->frame_count = frame_count;
    fdata->fighter_count = fighter_count;

    fdata->file.address = NULL;
    fdata->file.size = 0;

    return 0;
}

int
frame_data_load(struct frame_data* fdata, int game_id)
{
    int f;
    struct mstream ms;
    char file_name[64];
    void* mem;
    uint8_t major, minor;
    mem_size ptr_size, fighter_size;

    sprintf(file_name, "fdata/%d.fdat", game_id);
    if (mfile_map(&fdata->file, file_name) < 0)
        goto  map_file_failed;
    ms = mstream_from_memory(fdata->file.address, fdata->file.size);

    const char* magic = (const char*)mstream_read(&ms, 4);
    if (memcmp(magic, "FDAT", 4))
        goto wrong_magic;

    major = mstream_read_u8(&ms);
    minor = mstream_read_u8(&ms);
    if (major != 2 || minor != 0)
        goto wrong_version;

    mstream_read_u8(&ms);
    fdata->fighter_count = mstream_read_u8(&ms);
    fdata->frame_count = (int)mstream_read_lu32(&ms);
    mstream_read_lu32(&ms);

    ptr_size = sizeof(void*) * 12 * (mem_size)fdata->fighter_count;
    fighter_size = frame_size * (mem_size)fdata->frame_count;
    mem = mem_alloc(ptr_size);
    if (mem == NULL)
        goto alloc_buffer_failed;
    init_pointers(mem, fdata, fdata->fighter_count);

    for (f = 0; f != fdata->fighter_count; ++f)
    {
        fdata->timestamp[f]   = (uint64_t*)align_64((char*)mstream_ptr(&ms) + (mem_size)f * fighter_size);
        fdata->motion[f]      = (uint64_t*)(fdata->timestamp[f] + (mem_size)fdata->frame_count);
        fdata->frames_left[f] = (uint32_t*)(fdata->motion[f]    + (mem_size)fdata->frame_count);
        fdata->posx[f]        = (float*)(fdata->frames_left[f]  + (mem_size)fdata->frame_count);
        fdata->posy[f]        = (float*)(fdata->posx[f]         + (mem_size)fdata->frame_count);
        fdata->damage[f]      = (float*)(fdata->posy[f]         + (mem_size)fdata->frame_count);
        fdata->hitstun[f]     = (float*)(fdata->damage[f]       + (mem_size)fdata->frame_count);
        fdata->shield[f]      = (float*)(fdata->hitstun[f]      + (mem_size)fdata->frame_count);
        fdata->status[f]      = (uint16_t*)(fdata->shield[f]    + (mem_size)fdata->frame_count);
        fdata->hit_status[f]  = (uint8_t*)(fdata->status[f]     + (mem_size)fdata->frame_count);
        fdata->stocks[f]      = (uint8_t*)(fdata->hit_status[f]     + (mem_size)fdata->frame_count);
        fdata->flags[f]       = (uint8_t*)(fdata->stocks[f]     + (mem_size)fdata->frame_count);
    }

    return 0;

alloc_buffer_failed:
wrong_version:
wrong_magic:
    mfile_unmap(&fdata->file);
map_file_failed:
    return -1;
}

int
frame_data_save(const struct frame_data* fdata, int game_id)
{
    char file_name[64];
    FILE* fp;
    int f;
    char magic[4] = {'F', 'D', 'A', 'T'};
    uint8_t major = 2;
    uint8_t minor = 0;
    uint8_t pad1 = 0;
    uint8_t fighter_count = (uint8_t)fdata->fighter_count;
    uint32_t frame_count = (uint32_t)fdata->frame_count;
    uint32_t pad2 = 0;

    sprintf(file_name, "fdata/%d.fdat", game_id);
    fp = fopen_utf8_wb(file_name, (int)strlen(file_name));
    if (fp == NULL)
        return -1;

    fwrite(magic, 1, 4, fp);
    fwrite(&major, 1, 1, fp);
    fwrite(&minor, 1, 1, fp);
    fwrite(&pad1, 1, 1, fp);
    fwrite(&fighter_count, 1, 1, fp);
    fwrite(&frame_count, sizeof(frame_count), 1, fp);
    fwrite(&pad2, 1, 4, fp);

    for (f = 0; f != fdata->fighter_count; ++f)
    {
        fwrite(fdata->timestamp[f],   sizeof(uint64_t), (size_t)fdata->frame_count, fp);
        fwrite(fdata->motion[f],      sizeof(uint64_t), (size_t)fdata->frame_count, fp);
        fwrite(fdata->frames_left[f], sizeof(uint32_t), (size_t)fdata->frame_count, fp);
        fwrite(fdata->posx[f],        sizeof(float), (size_t)fdata->frame_count, fp);
        fwrite(fdata->posy[f],        sizeof(float), (size_t)fdata->frame_count, fp);
        fwrite(fdata->damage[f],      sizeof(float), (size_t)fdata->frame_count, fp);
        fwrite(fdata->hitstun[f],     sizeof(float), (size_t)fdata->frame_count, fp);
        fwrite(fdata->shield[f],      sizeof(float), (size_t)fdata->frame_count, fp);
        fwrite(fdata->status[f],      sizeof(uint16_t), (size_t)fdata->frame_count, fp);
        fwrite(fdata->hit_status[f],  sizeof(uint8_t), (size_t)fdata->frame_count, fp);
        fwrite(fdata->stocks[f],      sizeof(uint8_t), (size_t)fdata->frame_count, fp);
        fwrite(fdata->flags[f],       sizeof(uint8_t), (size_t)fdata->frame_count, fp);

        long target = (long)align_64((void*)ftell(fp));
        uint8_t dummy = 0xAA;
        while (ftell(fp) != target)
            fwrite(&dummy, 1, 1, fp);
    }

    fclose(fp);
    return 0;
}

void
frame_data_free(struct frame_data* fdata)
{
    if (fdata->file.size)
        mfile_unmap(&fdata->file);
    mem_free((void*)fdata->timestamp);
}
