#include "vh/backtrace.h"
#include "vh/hm.h"
#include "vh/hash.h"
#include "vh/log.h"
#include "vh/mem.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#define BACKTRACE_OMIT_COUNT 2

struct state
{
    struct hm report;
    mem_size allocations;
    mem_size deallocations;
    mem_size bytes_in_use;
    mem_size bytes_in_use_peak;
    unsigned ignore_malloc : 1;
};

struct report_info
{
    uintptr_t location;
    mem_size size;
#   if defined(VH_MEM_BACKTRACE)
    int backtrace_size;
    char** backtrace;
#   endif
};

static VH_THREADLOCAL struct state state;

/* ------------------------------------------------------------------------- */
static int report_info_cmp(const void* a, const void* b, int size) { return memcmp(a, b, (size_t)size); }
int
mem_threadlocal_init(void)
{
    state.allocations = 0;
    state.deallocations = 0;
    state.bytes_in_use = 0;
    state.bytes_in_use_peak = 0;

    /*
     * Hashmap will call mem_alloc during init, need to ignore this to avoid
     * crashing.
     */
    state.ignore_malloc = 1;
        if (hm_init_with_options(
            &state.report,
            sizeof(uintptr_t),
            sizeof(struct report_info),
            4096,
            hash32_ptr,
            report_info_cmp) != 0)
        {
            return -1;
        }
    state.ignore_malloc = 0;

    return 0;
}

/* ------------------------------------------------------------------------- */
#if defined(VH_MEM_BACKTRACE)
static void
print_backtrace(void)
{
    char** bt;
    int bt_size, i;

    if (state.ignore_malloc)
        return;

    if (!(bt = backtrace_get(&bt_size)))
    {
        log_mem_warn("Failed to generate backtrace\n");
        return;
    }

    for (i = BACKTRACE_OMIT_COUNT; i < bt_size; ++i)
    {
        if (strstr(bt[i], "invoke_main"))
            break;
        log_mem_warn("  %s\n", bt[i]);
    }
    backtrace_free(bt);
}
#else
#   define print_backtrace()
#endif

/* ------------------------------------------------------------------------- */
static void
track_allocation(uintptr_t addr, mem_size size)
{
    struct report_info* info;
    ++state.allocations;

    if (size == 0)
    {
        log_mem_warn("malloc(0)\n");
#if defined(VH_MEM_BACKTRACE)
        print_backtrace();
#endif
    }

    if (state.ignore_malloc)
        return;

    /*
     * Record allocation info. Call to hashmap and backtrace_get() may allocate
     * memory, so set flag to ignore the call to malloc() when inserting.
     */
    state.bytes_in_use += size;
    if (state.bytes_in_use_peak < state.bytes_in_use)
        state.bytes_in_use_peak = state.bytes_in_use;

    state.ignore_malloc = 1;
        /* insert info into hashmap */
        switch (hm_insert(&state.report, &addr, (void**)&info))
        {
            case 1: break;
            case 0:
                log_mem_err("Double allocation! This is usually caused by calling mem_track_allocation() on the same address twice.\n");
                print_backtrace();
                break;
            default: log_mem_err("Hashmap insert failed! Expect to see incorrect memory leak reports!\n");
         }

        /* record the location and size of the allocation */
        info->location = addr;
        info->size = size;

        /* Create backtrace to this allocation */
#if defined(VH_MEM_BACKTRACE)
        if (!(info->backtrace = backtrace_get(&info->backtrace_size)))
            log_mem_warn("Failed to generate backtrace\n");
#endif
    state.ignore_malloc = 0;

    return;
}

static void
track_deallocation(uintptr_t addr, const char* free_type)
{
    struct report_info* info;
    state.deallocations++;

    if (addr == 0)
    {
        log_mem_warn("free(NULL)\n");
#if defined(VH_MEM_BACKTRACE)
        print_backtrace();
#endif
    }

    if (state.ignore_malloc)
        return;

    /* find matching allocation and remove from hashmap */
    info = hm_erase(&state.report, &addr);
    if (info)
    {
        state.bytes_in_use -= info->size;
#if defined(VH_MEM_BACKTRACE)
        if (info->backtrace)
            backtrace_free(info->backtrace);
        else
            log_mem_warn("Allocation didn't have a backtrace (it was NULL)\n");
#endif
    }
    else
    {
        log_mem_warn("%s'ing something that was never allocated\n", free_type);
#if defined(VH_MEM_BACKTRACE)
        print_backtrace();
#endif
    }
}

/* ------------------------------------------------------------------------- */
void*
mem_alloc(mem_size size)
{
    void* p = malloc(size);
    if (p == NULL)
    {
        log_mem_err("malloc() failed (out of memory)\n");
#if defined(VH_MEM_BACKTRACE)
        print_backtrace();  /* probably won't work but may as well*/
#endif
        return NULL;
    }

    track_allocation((uintptr_t)p, size);
    return p;
}

/* ------------------------------------------------------------------------- */
void*
mem_realloc(void* p, mem_size new_size)
{
    uintptr_t old_addr = (uintptr_t)p;
    p = realloc(p, new_size);

    if (old_addr)
        track_deallocation(old_addr, "realloc()");

    if (p == NULL)
    {
        log_mem_err("realloc() failed (out of memory)\n");
#if defined(VH_MEM_BACKTRACE)
        print_backtrace();  /* probably won't work but may as well*/
#endif
        return NULL;
    }

    track_allocation((uintptr_t)p, new_size);
    return p;
}

/* ------------------------------------------------------------------------- */
void
mem_free(void* p)
{
    track_deallocation((uintptr_t)p, "free()");
    free(p);
}

/* ------------------------------------------------------------------------- */
mem_size
mem_threadlocal_deinit(void)
{
    uintptr_t leaks;

    --state.allocations; /* this is the single allocation still held by the report hashmap */

    log_mem_note("Memory report:\n");

    /* report details on any g_allocations that were not de-allocated */
    HM_FOR_EACH(&state.report, void*, struct report_info, key, info)
        log_mem_err("  un-freed memory at %" PRIx64 ", size %" PRIx32 "\n",
                info->location, info->size);

#if defined(VH_MEM_BACKTRACE)
        {
            int i;
            for (i = BACKTRACE_OMIT_COUNT; i < info->backtrace_size; ++i)
            {
                if (strstr(info->backtrace[i], "invoke_main"))
                    break;
                log_mem_err("    %s\n", info->backtrace[i]);
            }
        }
        backtrace_free(info->backtrace); /* this was allocated when malloc() was called */
#endif
    HM_END_EACH

    /* overall report */
    leaks = (state.allocations > state.deallocations ?
        state.allocations - state.deallocations :
        state.deallocations - state.allocations);
    log_mem_note("  allocations   : %" PRIu32 "\n", state.allocations);
    log_mem_note("  deallocations : %" PRIu32 "\n", state.deallocations);
    log_mem_note("  memory leaks  : %" PRIu64 "\n", leaks);
    log_mem_note("  peak memory   : %" PRIu32 " bytes\n", state.bytes_in_use_peak);

    ++state.allocations; /* this is the single allocation still held by the report hashmap */
    state.ignore_malloc = 1;
        hm_deinit(&state.report);
    state.ignore_malloc = 0;

    return (mem_size)leaks;
}

void
mem_track_allocation(void* p)
{
    track_allocation((uintptr_t)p, 1);
}

void
mem_track_deallocation(void* p)
{
    track_deallocation((uintptr_t)p, "track_deallocation()");
}
