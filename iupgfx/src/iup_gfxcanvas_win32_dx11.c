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
#include "iupgfx.h"

#include "iup_attrib.h"
#include "iup_class.h"

#include <stdio.h>
#include <string.h>

struct Vertex
{
    FLOAT x, y;
};

struct Constants
{
    FLOAT canvas_size[2];
    FLOAT aspect[2];
    FLOAT offset[2];
};

static struct Vertex quad_mesh[4] = {
    { -1.0,  1.0 },
    {  1.0,  1.0 },
    { -1.0, -1.0 },
    {  1.0, -1.0 },
};
static D3D11_INPUT_ELEMENT_DESC quad_layout[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const char* vs_quad = "\
float4 vs_yuv420(float2 pos_in : POSITION) : SV_POSITION\n\
{\n\
   return float4(pos_in.xy, 0.0, 1.0);\n\
}";

static const char* ps_yuv420 = "\
Texture2D tex0           : register(t0);\n\
SamplerState tex_sampler : register(s0);\n\
cbuffer constants        : register(b0)\n\
{\n\
    float2 canvas_size;\n\
    float2 aspect;\n\
    float2 offset;\n\
};\n\
float4 ps_yuv420(float4 pos : SV_POSITION) : SV_TARGET\n\
{\n\
    float2 uv = pos.xy + offset;\n\
    uv = uv / canvas_size * aspect;\n\
    float3 col0 = tex0.Sample(tex_sampler, uv).rgb;\n\
    return float4(col0, 1.0);\n\
}";

typedef struct Context3D
{
    HWND hWnd;
    IDXGISwapChain* swapchain;
    ID3D11Device* dev;
    ID3D11DeviceContext* devcon;
    ID3D11RenderTargetView* backbuffer;
    ID3D11Buffer* quad_vb;
    ID3D11Buffer* cbuffer;
    ID3D11InputLayout* quad_layout;
    ID3D11VertexShader* vs;
    ID3D11PixelShader* ps;
    ID3D11SamplerState* sampler_state;
    ID3D11Texture2D* textures[8];
    ID3D11ShaderResourceView* texture_views[8];

    int canvas_width, canvas_height, texture_width, texture_height;
    char texture_format[8];
} Context3D;

static int CreateDX11Context(Ihandle* ih, Context3D* ctx)
{
    HRESULT hResult;

    {
        ID3D11Device* base_dev;
        ID3D11DeviceContext* base_devcon;
        D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };
        UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(DEBUG)
        creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        hResult = D3D11CreateDevice(NULL,
            D3D_DRIVER_TYPE_HARDWARE,
            0,
            creation_flags,
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            &base_dev,
            NULL,
            &base_devcon);
        if (FAILED(hResult))
        {
            iupAttribSet(ih, "ERROR", "Failed to create D3D11 device");
            iupAttribSetStr(ih, "LASTERROR", IupGetGlobal("LASTERROR"));
            return IUP_NOERROR;
        }

        /* Get 1.0 interface of D3D11 device and context */
        hResult = ID3D11Device_QueryInterface(base_dev, &IID_ID3D11Device, &ctx->dev);
        if (FAILED(hResult))
        {
            iupAttribSet(ih, "ERROR", "Failed to get D3D11 1.1 device");
            iupAttribSetStr(ih, "LASTERROR", IupGetGlobal("LASTERROR"));
            log_err("%s\n", IupGetGlobal("LASTERROR"));
            return IUP_NOERROR;
        }

        hResult = ID3D11DeviceContext_QueryInterface(base_devcon, &IID_ID3D11DeviceContext, &ctx->devcon);
        if (FAILED(hResult))
        {
            iupAttribSet(ih, "ERROR", "Failed to get D3D11 1.1 device context");
            iupAttribSetStr(ih, "LASTERROR", IupGetGlobal("LASTERROR"));
            log_err("%s\n", IupGetGlobal("LASTERROR"));
            return IUP_NOERROR;
        }
    }

#if defined(DEBUG)
    ID3D11Debug* d3d_debug = NULL;
    ID3D11Device_QueryInterface(ctx->dev, &IID_ID3D11Debug, &d3d_debug);
    if (d3d_debug)
    {
        ID3D11InfoQueue* info_queue = NULL;
        if (SUCCEEDED(ID3D11Debug_QueryInterface(d3d_debug, &IID_ID3D11InfoQueue, &info_queue)))
        {
            ID3D11InfoQueue_SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, 1);
            ID3D11InfoQueue_SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_ERROR, 1);
            ID3D11InfoQueue_Release(info_queue);
        }
        ID3D11Debug_Release(d3d_debug);
    }
#endif

    /* Get DXGI factory needed to create swap chain */
    {
        IDXGIFactory1* dxgi_factory;
        {
            IDXGIDevice1* dxgi_dev;
            ID3D11Device_QueryInterface(ctx->dev, &IID_IDXGIDevice1, &dxgi_dev);

            IDXGIAdapter* adapter;
            IDXGIDevice_GetAdapter(dxgi_dev, &adapter);
            IDXGIDevice_Release(dxgi_dev);

            DXGI_ADAPTER_DESC ad;
            IDXGIAdapter_GetDesc(adapter, &ad);

            IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory1, &dxgi_factory);
            IDXGIAdapter_Release(adapter);
        }

        DXGI_SWAP_CHAIN_DESC scd;
        ZeroMemory(&scd, sizeof(scd));
        scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        scd.SampleDesc.Count = 1;
        scd.SampleDesc.Quality = 0;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.BufferCount = 1;
        scd.OutputWindow = ctx->hWnd;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        scd.Windowed = TRUE;
        hResult = IDXGIFactory1_CreateSwapChain(dxgi_factory, (IUnknown*)ctx->dev, &scd, &ctx->swapchain);
        if (FAILED(hResult))
        {
            iupAttribSet(ih, "ERROR", "Failed to get D3D11 device");
            iupAttribSetStr(ih, "LASTERROR", IupGetGlobal("LASTERROR"));
            log_err("%s\n", IupGetGlobal("LASTERROR"));
            return IUP_NOERROR;
        }
        IDXGIFactory1_Release(dxgi_factory);
    }

    /* Create framebuffer render target */
    {
        ID3D11Texture2D* backbuffer;
        IDXGISwapChain_GetBuffer(ctx->swapchain, 0, &IID_ID3D11Texture2D, (LPVOID*)&backbuffer);

        ID3D11Device_CreateRenderTargetView(ctx->dev, (ID3D11Resource*)backbuffer, NULL, &ctx->backbuffer);
        ID3D11Texture2D_Release(backbuffer);
    }

    /* set the render target as the back buffer */
    ID3D11DeviceContext_OMSetRenderTargets(ctx->devcon, 1, &ctx->backbuffer, NULL);

    /* Compile shaders */
    ID3DBlob *vs_blob, *ps_blob, *errors;
    if (D3DCompile(vs_quad, strlen(vs_quad), "vs_yuv420", NULL, NULL, "vs_yuv420", "vs_4_0", 0, 0, &vs_blob, &errors) != 0)
    {
        iupAttribSet(ih, "ERROR", "Failed to create device and swap chain.");
        iupAttribSetStr(ih, "LASTERROR", ID3D10Blob_GetBufferPointer(errors));
        log_err("%s\n", ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
    if (D3DCompile(ps_yuv420, strlen(ps_yuv420), "ps_yuv420", NULL, NULL, "ps_yuv420", "ps_4_0", 0, 0, &ps_blob, &errors) != 0)
    {
        iupAttribSet(ih, "ERROR", "Failed to create device and swap chain.");
        iupAttribSetStr(ih, "LASTERROR", ID3D10Blob_GetBufferPointer(errors));
        log_err("%s\n", ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
    if (ID3D11Device_CreateVertexShader(ctx->dev, ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob), NULL, &ctx->vs) != 0)
        log_err("create vs failed\n");
    if (ID3D11Device_CreatePixelShader(ctx->dev, ID3D10Blob_GetBufferPointer(ps_blob), ID3D10Blob_GetBufferSize(ps_blob), NULL, &ctx->ps) != 0)
        log_err("create ps failed\n");
    ID3D11DeviceContext_VSSetShader(ctx->devcon, ctx->vs, 0, 0);
    ID3D11DeviceContext_PSSetShader(ctx->devcon, ctx->ps, 0, 0);

    if (ID3D11Device_CreateInputLayout(ctx->dev, quad_layout, 1, ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob), &ctx->quad_layout) != 0)
        log_err("create layout failed\n");
    ID3D11DeviceContext_IASetInputLayout(ctx->devcon, ctx->quad_layout);

    /* Create quad vertex buffer */
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.ByteWidth = sizeof(quad_mesh);         /* Size of vertex buffer */
    bd.Usage = D3D11_USAGE_DEFAULT;           /* write access access by CPU and GPU */
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;  /* use as a vertex buffer */

    D3D11_SUBRESOURCE_DATA sd;
    sd.pSysMem = quad_mesh;
    sd.SysMemPitch = 0;
    sd.SysMemSlicePitch = 0;

    if (ID3D11Device_CreateBuffer(ctx->dev, &bd, &sd, &ctx->quad_vb) != 0)
        log_err("create buffer failed\n");

    /* Constant buffer */
    D3D11_BUFFER_DESC cbuffer_desc;
    ZeroMemory(&cbuffer_desc, sizeof cbuffer_desc);
    cbuffer_desc.ByteWidth = sizeof(struct Constants) + 0xF & 0xFFFFFFF0;  /* ByteWidth must be a multiple of 16, per the docs */
    cbuffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    cbuffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbuffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hResult = ID3D11Device_CreateBuffer(ctx->dev, &cbuffer_desc, NULL, &ctx->cbuffer);
    if (FAILED(hResult))
    {
        log_err("Failed to create constant buffer\n");
    }

    D3D11_SAMPLER_DESC sampler_desc;
    ZeroMemory(&sampler_desc, sizeof sampler_desc);
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sampler_desc.BorderColor[0] = 1.0f;
    sampler_desc.BorderColor[1] = 1.0f;
    sampler_desc.BorderColor[2] = 1.0f;
    sampler_desc.BorderColor[3] = 1.0f;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

    ID3D11Device_CreateSamplerState(ctx->dev, &sampler_desc, &ctx->sampler_state);

    iupAttribSet(ih, "ERROR", NULL);
    ID3D10Blob_Release(vs_blob);
    ID3D10Blob_Release(ps_blob);
    return IUP_NOERROR;
}

static void DestroyDX11Context(Context3D* ctx)
{
    ID3D11PixelShader_Release(ctx->ps);
    ID3D11VertexShader_Release(ctx->vs);
    ID3D11RenderTargetView_Release(ctx->backbuffer);
    IDXGISwapChain_Release(ctx->swapchain);
    ID3D11Device_Release(ctx->dev);
    ID3D11DeviceContext_Release(ctx->devcon);
}

static int CanvasDefaultResize_CB(Ihandle* ih, int width, int height)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");

    ID3D11DeviceContext_OMSetRenderTargets(ctx->devcon, 0, 0, 0);
    ID3D11RenderTargetView_Release(ctx->backbuffer);
    ID3D11DeviceContext_Flush(ctx->devcon);

    switch (IDXGISwapChain_ResizeBuffers(ctx->swapchain, 1, width, height, DXGI_FORMAT_UNKNOWN, 0))
    {
    case 0: break;
    case DXGI_ERROR_DEVICE_REMOVED:
    case DXGI_ERROR_DEVICE_RESET:
        log_err("device removed/reset\n");
        break;
    default:
        log_err("Failed to resize\n");
        break;
    }

    {
        ID3D11Texture2D* backbuffer;
        IDXGISwapChain_GetBuffer(ctx->swapchain, 0, &IID_ID3D11Texture2D, &backbuffer);

        ID3D11Device_CreateRenderTargetView(ctx->dev, (ID3D11Resource*)backbuffer, NULL, &ctx->backbuffer);
        ID3D11Texture2D_Release(backbuffer);
    }

    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
    viewport.Width = ctx->canvas_width = width;
    viewport.Height = ctx->canvas_height = height;
    ID3D11DeviceContext_RSSetViewports(ctx->devcon, 1, &viewport);

    return IUP_DEFAULT;
}

static int CanvasDefaultRedraw_CB(Ihandle* ih, float x, float y)
{
    HRESULT hResult;
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");

    ID3D11DeviceContext_OMSetRenderTargets(ctx->devcon, 1, &ctx->backbuffer, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx->devcon, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_IASetInputLayout(ctx->devcon, ctx->quad_layout);

    ID3D11DeviceContext_VSSetShader(ctx->devcon, ctx->vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(ctx->devcon, ctx->ps, NULL, 0);

    UINT stride = sizeof(struct Vertex);
    UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(ctx->devcon, 0, 1, &ctx->quad_vb, &stride, &offset);

    D3D11_MAPPED_SUBRESOURCE mapped_subresource;
    hResult = ID3D11DeviceContext_Map(ctx->devcon, (ID3D11Resource*)ctx->cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
    if (SUCCEEDED(hResult))
    {
        struct Constants* constants = (struct Constants*)(mapped_subresource.pData);
        constants->offset[0] = 0.0;
        constants->offset[1] = 0.0;
        constants->aspect[0] = 1.0;
        constants->aspect[1] = 1.0;

        FLOAT canvas_ar = (FLOAT)ctx->canvas_width / (FLOAT)ctx->canvas_height;
        FLOAT texture_ar = (FLOAT)ctx->texture_width / (FLOAT)ctx->texture_height;
        if (canvas_ar > texture_ar)
        {
            constants->offset[0] = ((FLOAT)ctx->canvas_height * texture_ar - ctx->canvas_width) / 2.0;
            constants->aspect[0] = canvas_ar / texture_ar;
        }
        else if (canvas_ar < texture_ar)
        {
            constants->offset[1] = ((FLOAT)ctx->canvas_width / texture_ar - (FLOAT)ctx->canvas_height) / 2.0;
            constants->aspect[1] = texture_ar / canvas_ar;
        }

        constants->canvas_size[0] = (FLOAT)ctx->canvas_width;
        constants->canvas_size[1] = (FLOAT)ctx->canvas_height;
        ID3D11DeviceContext_Unmap(ctx->devcon, (ID3D11Resource*)ctx->cbuffer, 0);
        ID3D11DeviceContext_PSSetConstantBuffers(ctx->devcon, 0, 1, &ctx->cbuffer);
    }

    ID3D11DeviceContext_PSSetShaderResources(ctx->devcon, 0, 1, &ctx->texture_views[0]);
    ID3D11DeviceContext_PSSetSamplers(ctx->devcon, 0, 1, &ctx->sampler_state);

    ID3D11DeviceContext_Draw(ctx->devcon, 4, 0);
    IDXGISwapChain_Present(ctx->swapchain, 0, 0);

    return IUP_DEFAULT;
}

static int CanvasCreateMethod(Ihandle* ih, void** params)
{
    Context3D* ctx;
    (void)params;

    ctx = mem_alloc(sizeof *ctx);
    ZeroMemory(ctx, sizeof * ctx);
    iupAttribSet(ih, "_IUP_D3D11CONTEXT", (char*)ctx);

    IupSetCallback(ih, "RESIZE_CB", (Icallback)CanvasDefaultResize_CB);
    IupSetCallback(ih, "ACTION", (Icallback)CanvasDefaultRedraw_CB);

    return IUP_NOERROR;
}

static int CanvasMapMethod(Ihandle* ih)
{
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
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    DestroyDX11Context(ctx);
}

static void CanvasDestroyMethod(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    mem_free(ctx);
    iupAttribSet(ih, "_IUP_D3D11CONTEXT", NULL);
}

/* -------------- Private API ---------------------------------------------- */

int iupdrvGfxInit(void)
{
    return 0;
}

void iupdrvGfxDeInit(void)
{
}

void iupdrvGfxCanvasInitClass(Iclass* ic)
{
    ic->Create = CanvasCreateMethod;
    ic->Map = CanvasMapMethod;
    ic->UnMap = CanvasUnMapMethod;
    ic->Destroy = CanvasDestroyMethod;
}

int iupdrvGfxCanvasGetTexWidth(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    return ctx->texture_width;
}

int iupdrvGfxCanvasGetTexHeight(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    return ctx->texture_height;
}

void iupdrvGfxCanvasSetTexSize(Ihandle* ih, int width, int height)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    ctx->texture_width = width;
    ctx->texture_height = height;

    for (int i = 0; i != 8; ++i)
        if (ctx->textures[i])
        {
            ID3D11ShaderResourceView_Release(ctx->texture_views[i]);
            ID3D11Texture2D_Release(ctx->textures[i]);
            ctx->texture_views[i] = NULL;
            ctx->textures[i] = NULL;
        }
}

void iupdrvGfxCanvasSetTexRGBA(Ihandle* ih, int id, const char* value)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");

    if (id > 7)
    {
        char buf[64];
        snprintf(buf, 64, "Only support 8 textures (requested index %d)", id);
        iupAttribSet(ih, "ERROR", buf);
        log_err("%s\n", buf);
        return;
    }
    if (id == IUP_INVALID_ID)
    {
        id = 0;
    }
    else if (id < 0)
    {
        log_err("Invalid ID was passed\n");
        return;
    }

    if (ctx->textures[id] == NULL)
    {
        D3D11_TEXTURE2D_DESC texture_desc;
        ZeroMemory(&texture_desc, sizeof texture_desc);
        texture_desc.Width = ctx->texture_width;
        texture_desc.Height = ctx->texture_height;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Usage = D3D11_USAGE_DYNAMIC;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA texture_subresource;
        ZeroMemory(&texture_subresource, sizeof texture_subresource);
        texture_subresource.pSysMem = value;
        texture_subresource.SysMemPitch = 4 * ctx->texture_width;

        HRESULT hResult = ID3D11Device_CreateTexture2D(ctx->dev, &texture_desc, &texture_subresource, &ctx->textures[id]);
        if (FAILED(hResult))
        {
            log_err("Failed to create texture\n");
        }

        hResult = ID3D11Device_CreateShaderResourceView(ctx->dev, (ID3D11Resource*)ctx->textures[id], NULL, &ctx->texture_views[id]);
        if (FAILED(hResult))
        {
            log_err("Failed to create texture view\n");
        }
    }
    else
    {
        D3D11_MAPPED_SUBRESOURCE mapped_subresource;
        HRESULT hResult = ID3D11DeviceContext_Map(ctx->devcon, (ID3D11Resource*)ctx->textures[id], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
        if (SUCCEEDED(hResult))
        {
            memcpy(mapped_subresource.pData, value, ctx->texture_width * ctx->texture_height * 4);
            ID3D11DeviceContext_Unmap(ctx->devcon, (ID3D11Resource*)ctx->textures[id], 0);
        }
    }
}

/* -------------- Public API ----------------------------------------------- */

int IupGfxIsCurrent(Ihandle* ih)
{
    (void)ih;
    return 1;
}

void IupGfxMakeCurrent(Ihandle* ih)
{
    (void)ih;
}
