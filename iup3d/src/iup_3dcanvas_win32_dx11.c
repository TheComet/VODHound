#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#include "vh/mem.h"
#include "vh/log.h"

#include "iup.h"
#include "iupcbs.h"
#include "iup3d.h"

#include "iup_attrib.h"
#include "iup_class.h"

struct Vertex
{
    FLOAT x, y;
};

static struct Vertex quad[6] = {
    {0.0, 0.0},
    {1.0, 1.0},
    {1.0, 0.0},
    {0.0, 0.0},
    {0.0, 1.0},
    {1.0, 1.0}
};
static D3D11_INPUT_ELEMENT_DESC quad_layout[] =
{
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
};

static const char* vs_yuv420 =
"struct VSOut\n"
"{\n"
"   float2 position : SV_POSITION;\n"
"};\n"
"\n"
"float4 vs_yuv420(float2 pos_in : POSITION) : SV_POSITION\n"
"{\n"
"   return float4(pos_in, 0.0, 1.0);\n"
"}\n";

static const char* ps_yuv420 =
"float4 ps_yuv420(float4 pos : SV_POSITION) : SV_TARGET\n"
"{\n"
"   return pos;\n"
"}\n";

typedef struct Context3D
{
    HWND hWnd;
    IDXGISwapChain* swapchain;
    ID3D11Device* dev;
    ID3D11DeviceContext* devcon;
    ID3D11RenderTargetView* backbuffer;
    ID3D11Buffer* quad;
    ID3D11InputLayout* quad_layout;
    ID3D11VertexShader* vs;
    ID3D11PixelShader* ps;
} Context3D;

static void report_and_release_compile_errors(ID3DBlob* errors)
{
    log_err("%.*s\n", ID3D10Blob_GetBufferSize(errors), ID3D10Blob_GetBufferPointer(errors));
    ID3D10Blob_Release(errors);
}

static int CreateDX11Context(Ihandle* ih, Context3D* ctx)
{
    log_dbg("CreateDX11Context\n");

    DXGI_SWAP_CHAIN_DESC scd;
    ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));
    scd.BufferCount = 1;                                    /* one back buffer */
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     /* use 32-bit color */
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      /* how swap chain is to be used */
    scd.OutputWindow = ctx->hWnd;                           /* the window to be used */
    scd.SampleDesc.Count = 4;                               /* how many multisamples */
    scd.Windowed = TRUE;                                    /* windowed/full-screen mode */

    /* create a device, device context and swap chain using the information in the scd struct */
    D3D_FEATURE_LEVEL DX11 = D3D_FEATURE_LEVEL_11_0;
    if (D3D11CreateDeviceAndSwapChain(NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        D3D11_CREATE_DEVICE_DEBUG,
        &DX11,
        1,
        D3D11_SDK_VERSION,
        &scd,
        &ctx->swapchain,
        &ctx->dev,
        NULL,
        &ctx->devcon) != 0)
    {
        iupAttribSet(ih, "ERROR", "Failed to create device and swap chain.");
        iupAttribSetStr(ih, "LASTERROR", IupGetGlobal("LASTERROR"));
        return IUP_NOERROR;
    }

    ID3D11Texture2D* pBackBuffer;
    IDXGISwapChain_GetBuffer(ctx->swapchain, 0, &IID_ID3D11Texture2D, (LPVOID*)&pBackBuffer);

    // use the back buffer address to create the render target
    ID3D11Device_CreateRenderTargetView(ctx->dev, (ID3D11Resource*)pBackBuffer, NULL, &ctx->backbuffer);
    ID3D11Texture2D_Release(pBackBuffer);

    // set the render target as the back buffer
    ID3D11DeviceContext_OMSetRenderTargets(ctx->devcon, 1, &ctx->backbuffer, NULL);

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC;                /* write access access by CPU and GPU */
    bd.ByteWidth = sizeof(quad);                   /* size of vertex data */
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       /* use as a vertex buffer */
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    /* allow CPU to write in buffer */
    ID3D11Device_CreateBuffer(ctx->dev, &bd, NULL, &ctx->quad);

    ID3DBlob *vs, *ps, *errors;
    if (D3DCompile(vs_yuv420, strlen(vs_yuv420), "vs_yuv420", NULL, NULL, "vs_yuv420", "vs_4_0", 0, 0, &vs, &errors) != 0)
    {
        report_and_release_compile_errors(errors);
    }
    if (D3DCompile(ps_yuv420, strlen(ps_yuv420), "ps_yuv420", NULL, NULL, "ps_yuv420", "ps_4_0", 0, 0, &ps, &errors) != 0)
    {
        report_and_release_compile_errors(errors);
    }
    ID3D11Device_CreateVertexShader(ctx->dev, ID3D10Blob_GetBufferPointer(vs), ID3D10Blob_GetBufferSize(vs), NULL, &ctx->vs);
    ID3D11Device_CreatePixelShader(ctx->dev, ID3D10Blob_GetBufferPointer(ps), ID3D10Blob_GetBufferSize(ps), NULL, &ctx->ps);
    ID3D11DeviceContext_VSSetShader(ctx->devcon, ctx->vs, 0, 0);
    ID3D11DeviceContext_PSSetShader(ctx->devcon, ctx->ps, 0, 0);

    D3D11_MAPPED_SUBRESOURCE ms;
    ID3D11DeviceContext_Map(ctx->devcon, (ID3D11Resource*)ctx->quad, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, quad, sizeof(quad));
    ID3D11DeviceContext_Unmap(ctx->devcon, (ID3D11Resource*)ctx->quad, 0);

    ID3D11Device_CreateInputLayout(ctx->dev, quad_layout, 1, ID3D10Blob_GetBufferPointer(vs), ID3D10Blob_GetBufferSize(vs), &ctx->quad_layout);
    ID3D11DeviceContext_IASetInputLayout(ctx->devcon, ctx->quad_layout);

    iupAttribSet(ih, "ERROR", NULL);
    return IUP_NOERROR;
}

static void DestroyDX11Context(Context3D* ctx)
{
    log_dbg("DestroyDX11Context\n");
    ID3D11PixelShader_Release(ctx->ps);
    ID3D11VertexShader_Release(ctx->vs);
    ID3D11RenderTargetView_Release(ctx->backbuffer);
    IDXGISwapChain_Release(ctx->swapchain);
    ID3D11Device_Release(ctx->dev);
    ID3D11DeviceContext_Release(ctx->devcon);
}

static int CanvasDefaultResize_CB(Ihandle* ih, int width, int height)
{
    D3D11_VIEWPORT viewport;
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");

    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = width;
    viewport.Height = height;

    ID3D11DeviceContext_RSSetViewports(ctx->devcon, 1, &viewport);
    Iup3DSwapBuffers(ih);

    return IUP_DEFAULT;
}

static int CanvasCreateMethod(Ihandle* ih, void** params)
{
    log_dbg("CanvasCreateMethod\n");
    Context3D* ctx;
    (void)params;

    ctx = mem_alloc(sizeof *ctx);
    iupAttribSet(ih, "_IUP_D3D11CONTEXT", (char*)ctx);

    IupSetCallback(ih, "RESIZE_CB", (Icallback)CanvasDefaultResize_CB);

    return IUP_NOERROR;
}

static int CanvasMapMethod(Ihandle* ih)
{
    log_dbg("CanvasMapMethod\n");
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");

    /* get a device context */
    ctx->hWnd = (HWND)iupAttribGet(ih, "HWND"); /* check first in the hash table, can be defined by the IupFileDlg */
    if (!ctx->hWnd)
        ctx->hWnd = (HWND)IupGetAttribute(ih, "HWND");  /* works for Win32 and GTK, only after mapping the IupCanvas */
    if (!ctx->hWnd)
        return IUP_NOERROR;

    return CreateDX11Context(ih, ctx);
}

static void CanvasUnMapMethod(Ihandle* ih)
{
    log_dbg("CanvasUnMapMethod\n");
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    DestroyDX11Context(ctx);
}

static void CanvasDestroyMethod(Ihandle* ih)
{
    log_dbg("CanvasDestroyMethod\n");
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    mem_free(ctx);
    iupAttribSet(ih, "_IUP_D3D11CONTEXT", NULL);;
}

void iupdrv3DCanvasInitClass(Iclass* ic)
{
    ic->Create = CanvasCreateMethod;
    ic->Map = CanvasMapMethod;
    ic->UnMap = CanvasUnMapMethod;
    ic->Destroy = CanvasDestroyMethod;
}

int Iup3DIsCurrent(Ihandle* ih)
{
    (void)ih;
    return 1;
}

void Iup3DMakeCurrent(Ihandle* ih)
{
    (void)ih;
}

static FLOAT CLEAR_COLOR[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

void Iup3DSwapBuffers(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    log_dbg("Swap buffers\n");

    ID3D11DeviceContext_ClearRenderTargetView(ctx->devcon, ctx->backbuffer, CLEAR_COLOR);

    UINT stride = sizeof(struct Vertex);
    UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(ctx->devcon, 0, 1, &ctx->quad, &stride, &offset);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx->devcon, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_Draw(ctx->devcon, 6, 0);

    IDXGISwapChain_Present(ctx->swapchain, 0, 0);
}
