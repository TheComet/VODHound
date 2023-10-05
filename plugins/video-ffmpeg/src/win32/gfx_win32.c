#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>

#include "video-ffmpeg/canvas.h"
#include "video-ffmpeg/gfx.h"

#include "vh/log.h"
#include "vh/mem.h"
#include "vh/thread.h"

struct gfx
{
	struct thread thread;
	struct mutex mutex;
	char request_stop;

	IDXGISwapChain* swapchain;             // the pointer to the swap chain interface
	ID3D11Device* dev;                     // the pointer to our Direct3D device interface
	ID3D11DeviceContext* devcon;           // the pointer to our Direct3D device context
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
gfx_create(struct canvas* canvas)
{
	struct gfx* gfx = mem_alloc(sizeof *gfx);
	if (gfx == NULL)
		goto alloc_gfx_failed;

	// create a struct to hold information about the swap chain
	DXGI_SWAP_CHAIN_DESC scd;

	// clear out the struct for use
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

	// fill the swap chain description struct
	scd.BufferCount = 1;                                    // one back buffer
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
	scd.OutputWindow = canvas_get_native_handle(canvas);    // the window to be used
	scd.SampleDesc.Count = 4;                               // how many multisamples
	scd.Windowed = TRUE;                                    // windowed/full-screen mode

	// create a device, device context and swap chain using the information in the scd struct
	D3D11CreateDeviceAndSwapChain(NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		NULL,
		NULL,
		NULL,
		D3D11_SDK_VERSION,
		&scd,
		&gfx->swapchain,
		&gfx->dev,
		NULL,
		&gfx->devcon);

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
gfx_destroy(struct gfx* gfx, struct canvas* canvas)
{
	mutex_lock(gfx->mutex);
		gfx->request_stop = 1;
	mutex_unlock(gfx->mutex);
	thread_join(gfx->thread, 0);

	mutex_deinit(gfx->mutex);

	// close and release all existing COM objects
	/*
	gfx->swapchain->Release();
	gfx->dev->Release();
	gfx->devcon->Release();*/

	mem_free(gfx);
}
