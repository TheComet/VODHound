#include "vh/mem.h"
#include "vh/hm.h"
#include "vh/hash.h"
#include "vh/backtrace.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#define BACKTRACE_OMIT_COUNT 2

#if defined(VH_MEM_DEBUGGING)
static VH_THREADLOCAL mem_size g_allocations = 0;
static VH_THREADLOCAL mem_size d_deallocations = 0;
static VH_THREADLOCAL mem_size g_ignore_hm_malloc = 0;
VH_THREADLOCAL mem_size g_bytes_in_use = 0;
VH_THREADLOCAL mem_size g_bytes_in_use_peak = 0;
static VH_THREADLOCAL struct hm g_report;

typedef struct report_info_t
{
    void* location;
    mem_size size;
#   if defined(VH_MEM_BACKTRACE)
    int backtrace_size;
    char** backtrace;
#   endif
} report_info_t;

/* ------------------------------------------------------------------------- */
int
mem_threadlocal_init(void)
{
    g_allocations = 0;
    d_deallocations = 0;
    g_bytes_in_use = 0;
    g_bytes_in_use_peak = 0;

    /*
     * Hashmap will call malloc during init, need to ignore this to avoid
     * crashing.
     */
    g_ignore_hm_malloc = 1;
        if (hm_init_with_options(
            &g_report,
            sizeof(void*),
            sizeof(report_info_t),
            4096,
            hash32_ptr,
            (hm_compare_func)memcmp) != 0)
        {
            return -1;
        }
    g_ignore_hm_malloc = 0;

    return 0;
}

/* ------------------------------------------------------------------------- */
void*
mem_alloc(mem_size size)
{
    void* p = NULL;
    report_info_t info = {0};

    /* allocate */
    p = malloc(size);
    if (p == NULL)
        return NULL;

    ++g_allocations;

    /*
     * Record allocation info. Call to hashmap and backtrace_get() may allocate
     * memory, so set flag to ignore the call to malloc() when inserting.
     */
    if (!g_ignore_hm_malloc)
    {
        g_bytes_in_use += size;
        if (g_bytes_in_use_peak < g_bytes_in_use)
            g_bytes_in_use_peak = g_bytes_in_use;

        g_ignore_hm_malloc = 1;

            /* record the location and size of the allocation */
            info.location = p;
            info.size = size;

            /* if (enabled, generate a backtrace so we know where memory leaks
             * occurred */
#   if defined(VH_MEM_BACKTRACE)
            if (!(info.backtrace = backtrace_get(&info.backtrace_size)))
                fprintf(stderr, "[memory] WARNING: Failed to generate backtrace\n");
            if (size == 0)
            {
                int i;
                fprintf(stderr, "[memory] WARNING: malloc(0)");
                fprintf(stderr, "  -----------------------------------------\n");
                fprintf(stderr, "  backtrace to where malloc() was called:\n");
                for (i = 0; i < info.backtrace_size; ++i)
                    fprintf(stderr, "      %s\n", info.backtrace[i]);
                fprintf(stderr, "  -----------------------------------------\n");
            }
#   else
            if (size == 0)
                fprintf(stderr, "[memory] WARNING: malloc(0)");
#   endif

            /* insert info into hashmap */
            if (hm_insert_new(&g_report, &p, &info) != 1)
                fprintf(stderr, "[memory] Hashmap insert failed\n");

        g_ignore_hm_malloc = 0;
    }

    /* success */
    return p;
}

/* ------------------------------------------------------------------------- */
void*
mem_realloc(void* p, mem_size new_size)
{
    void* old_p = p;
    p = realloc(p, new_size);

    /* If old pointer is NULL, this behaves the same as a malloc */
    if (old_p == NULL)
    {
        ++g_allocations;
    }
    else
    {
        /* Remove old entry in report */
        report_info_t* info = (report_info_t*)hm_erase(&g_report, &old_p);
        if (info)
        {
            g_bytes_in_use -= info->size;

#   if defined(VH_MEM_BACKTRACE)
            if (info->backtrace)
                backtrace_free(info->backtrace);
            else
                fprintf(stderr, "[memory] WARNING: free(): Allocation didn't "
                    "have a backtrace (it was NULL)\n");
#   endif
        }
        else
        {
#   if defined(VH_MEM_BACKTRACE)
            char** bt;
            int bt_size, i;
            fprintf(stderr, "  -----------------------------------------\n");
#   endif
            fprintf(stderr, "[memory] WARNING: realloc(): Reallocating something that was never malloc'd");
#   if defined(VH_MEM_BACKTRACE)
            if ((bt = backtrace_get(&bt_size)))
            {
                fprintf(stderr, "  backtrace to where realloc() was called:\n");
                for (i = 0; i < bt_size; ++i)
                    fprintf(stderr, "      %s\n", bt[i]);
                fprintf(stderr, "  -----------------------------------------\n");
                backtrace_free(bt);
            }
            else
                fprintf(stderr, "[memory] WARNING: Failed to generate backtrace\n");
#   endif
        }
    }

    /*
     * Record allocation info. Call to hashmap and backtrace_get() may allocate
     * memory, so set flag to ignore the call to malloc() when inserting.
     */
    if (!g_ignore_hm_malloc)
    {
        report_info_t info = {0};

        g_bytes_in_use += new_size;
        if (g_bytes_in_use_peak < g_bytes_in_use)
            g_bytes_in_use_peak = g_bytes_in_use;

        g_ignore_hm_malloc = 1;

            /* record the location and size of the allocation */
            info.location = p;
            info.size = new_size;

            /* if (enabled, generate a backtrace so we know where memory leaks
            * occurred */
#   if defined(VH_MEM_BACKTRACE)
            if (!(info.backtrace = backtrace_get(&info.backtrace_size)))
                fprintf(stderr, "[memory] WARNING: Failed to generate backtrace\n");
#   endif

            /* insert info into hashmap */
            if (hm_insert_new(&g_report, &p, &info) != 1)
                fprintf(stderr, "[memory] Hashmap insert failed\n");

        g_ignore_hm_malloc = 0;
    }

    return p;
}

/* ------------------------------------------------------------------------- */
void
mem_free(void* p)
{
    /* find matching allocation and remove from hashmap */
    if (!g_ignore_hm_malloc)
    {
        report_info_t* info = (report_info_t*)hm_erase(&g_report, &p);
        if (info)
        {
            g_bytes_in_use -= info->size;
#   if defined(VH_MEM_BACKTRACE)
            if (info->backtrace)
                backtrace_free(info->backtrace);
            else
                fprintf(stderr, "[memory] WARNING: free(): Allocation didn't "
                    "have a backtrace (it was NULL)\n");
#   endif
        }
        else
        {
#   if defined(VH_MEM_BACKTRACE)
            char** bt;
            int bt_size, i;
            fprintf(stderr, "  -----------------------------------------\n");
#   endif
            fprintf(stderr, "  WARNING: Freeing something that was never allocated\n");
#   if defined(VH_MEM_BACKTRACE)
            if ((bt = backtrace_get(&bt_size)))
            {
                fprintf(stderr, "  backtrace to where free() was called:\n");
                for (i = 0; i < bt_size; ++i)
                    fprintf(stderr, "      %s\n", bt[i]);
                fprintf(stderr, "  -----------------------------------------\n");
                backtrace_free(bt);
            }
            else
                fprintf(stderr, "[memory] WARNING: Failed to generate backtrace\n");
#   endif
        }
    }

    if (p)
    {
        ++d_deallocations;
        free(p);
    }
    else
        fprintf(stderr, "Warning: free(NULL)\n");
}

/* ------------------------------------------------------------------------- */
mem_size
mem_threadlocal_deinit(void)
{
    uintptr_t leaks;

    --g_allocations; /* this is the single allocation still held by the report hashmap */

    fprintf(stderr, "=========================================\n");
    fprintf(stderr, "Memory Report\n");
    fprintf(stderr, "=========================================\n");

    /* report details on any g_allocations that were not de-allocated */
    if (hm_count(&g_report) != 0)
    {
        HM_FOR_EACH(&g_report, void*, report_info_t, key, info)

            fprintf(stderr, "  un-freed memory at %p, size %p\n", info->location, (void*)(uintptr_t)info->size);
            mem_mutated_string_and_hex_dump(info->location, info->size);

#   if defined(VH_MEM_BACKTRACE)
            fprintf(stderr, "  Backtrace to where malloc() was called:\n");
            {
                intptr_t i;
                for (i = BACKTRACE_OMIT_COUNT; i < info->backtrace_size; ++i)
                    fprintf(stderr, "    %s\n", info->backtrace[i]);
            }
            backtrace_free(info->backtrace); /* this was allocated when malloc() was called */
            fprintf(stderr, "  -----------------------------------------\n");
#   endif

        HM_END_EACH

        fprintf(stderr, "=========================================\n");
    }

    /* overall report */
    leaks = (g_allocations > d_deallocations ? g_allocations - d_deallocations : d_deallocations - g_allocations);
    fprintf(stderr, "allocations   : %" PRIu32 "\n", g_allocations);
    fprintf(stderr, "deallocations : %" PRIu32 "\n", d_deallocations);
    fprintf(stderr, "memory leaks  : %" PRIu64 "\n", leaks);
    fprintf(stderr, "peak memory usage: %" PRIu32 " bytes\n", g_bytes_in_use_peak);
    fprintf(stderr, "=========================================\n");

    ++g_allocations; /* this is the single allocation still held by the report hashmap */
    g_ignore_hm_malloc = 1;
        hm_deinit(&g_report);
    g_ignore_hm_malloc = 0;

    return leaks;
}

/* ------------------------------------------------------------------------- */
mem_size
mem_get_num_allocs(void)
{
    return hm_count(&g_report);
}

/* ------------------------------------------------------------------------- */
mem_size
mem_get_memory_usage(void)
{
    return g_bytes_in_use;
}

#else /* VH_MEM_DEBUGGING */

int mem_threadlocal_init(void)         { return 0; }
uintptr_t mem_threadlocal_deinit(void) { return 0; }
uintptr_t mem_get_num_allocs(void)     { return 0; }
uintptr_t mem_get_memory_usage(void)   { return 0; }

#endif /* VH_MEM_DEBUGGING */

/* ------------------------------------------------------------------------- */
void
mem_mutated_string_and_hex_dump(const void* data, mem_size length_in_bytes)
{
    char* dump;
    mem_idx i;

    /* allocate and copy data into new buffer */
    if (!(dump = malloc(length_in_bytes + 1)))
    {
        fprintf(stderr, "[memory] WARNING: Failed to malloc() space for dump\n");
        return;
    }
    memcpy(dump, data, length_in_bytes);
    dump[length_in_bytes] = '\0';

    /* mutate null terminators into dots */
    for (i = 0; i != (mem_idx)length_in_bytes; ++i)
        if (dump[i] == '\0')  /* valgrind will complain about conditional jump depending on uninitialized value here -- that's ok */
            dump[i] = '.';

    /* dump */
    fprintf(stderr, "  mutated string dump: %s\n", dump);
    fprintf(stderr, "  hex dump: ");
    for (i = 0; i != (mem_idx)length_in_bytes; ++i)
        fprintf(stderr, " %02x", (unsigned char)dump[i]);
    fprintf(stderr, "\n");

    free(dump);
}
