#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "video-ffmpeg/gfx.h"

#include "vh/log.h"
#include "vh/mem.h"
#include "vh/thread.h"

struct gfx
{
    struct thread thread;
    struct mutex mutex;
    char request_stop;
};

static void*
render_thread(void* args)
{
    struct gfx* gfx = args;

    mutex_lock(gfx->mutex);
    while (!gfx->request_stop)
    {
        mutex_unlock(gfx->mutex);
        mutex_lock(gfx->mutex);
    }
    mutex_unlock(gfx->mutex);

    return NULL;
}

struct gfx*
gfx_create(Ihandle* canvas)
{
    struct gfx* gfx = mem_alloc(sizeof *gfx);
    if (gfx == NULL)
        goto alloc_gfx_failed;

    mutex_init(&gfx->mutex);
    gfx->request_stop = 0;

    if (thread_start(&gfx->thread, render_thread, gfx) != 0)
        goto start_thread_failed;

    return gfx;

    start_thread_failed : mutex_deinit(gfx->mutex);
    init_mutex_failed   : mem_free(gfx);
    alloc_gfx_failed    : return NULL;
}

void
gfx_destroy(struct gfx* gfx, Ihandle* canvas)
{
    mutex_lock(gfx->mutex);
        gfx->request_stop = 1;
    mutex_unlock(gfx->mutex);
    thread_join(gfx->thread, 0);

    mutex_deinit(gfx->mutex);

    mem_free(gfx);
}
