#include <stdint.h>

struct frame_data
{
    uint64_t* motion;
    uint16_t* status;
};

int next_state(int idx, const struct frame_data* fdata)
{
    switch (fdata->motion[idx])
    {
        case 0xc3a4e2597: return 1;
        case 0xacbfc42e6: return 2;
        case 0xc3495ada5: return 3;
    }
    return 0;
}

