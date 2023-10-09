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

struct Vertex
{
    FLOAT x, y;
};

static struct Vertex quad[6] = {
    { -1.0, -1.0 },
    {  1.0,  1.0 },
    {  1.0, -1.0 },
    { -1.0, -1.0 },
    { -1.0,  1.0 },
    {  1.0,  1.0 },
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
float4 ps_yuv420(float4 pos : SV_POSITION) : SV_TARGET\n\
{\n\
    float2 uv = pos.xy * 0.5 + 0.5;\n\
    return float4(uv/500, 0.0, 1.0);\n\
}";

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

static int CreateDX11Context(Ihandle* ih, Context3D* ctx)
{
    HRESULT hResult;
    log_dbg("CreateDX11Context\n");

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

    // set the render target as the back buffer
    ID3D11DeviceContext_OMSetRenderTargets(ctx->devcon, 1, &ctx->backbuffer, NULL);

    ID3DBlob *vs, *ps, *errors;
    if (D3DCompile(vs_quad, strlen(vs_quad), "vs_yuv420", NULL, NULL, "vs_yuv420", "vs_4_0", 0, 0, &vs, &errors) != 0)
    {
        iupAttribSet(ih, "ERROR", "Failed to create device and swap chain.");
        iupAttribSetStr(ih, "LASTERROR", ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
    if (D3DCompile(ps_yuv420, strlen(ps_yuv420), "ps_yuv420", NULL, NULL, "ps_yuv420", "ps_4_0", 0, 0, &ps, &errors) != 0)
    {
        iupAttribSet(ih, "ERROR", "Failed to create device and swap chain.");
        iupAttribSetStr(ih, "LASTERROR", ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
    if (ID3D11Device_CreateVertexShader(ctx->dev, ID3D10Blob_GetBufferPointer(vs), ID3D10Blob_GetBufferSize(vs), NULL, &ctx->vs) != 0)
        log_err("create vs failed\n");
    if (ID3D11Device_CreatePixelShader(ctx->dev, ID3D10Blob_GetBufferPointer(ps), ID3D10Blob_GetBufferSize(ps), NULL, &ctx->ps) != 0)
        log_err("create ps failed\n");
    ID3D11DeviceContext_VSSetShader(ctx->devcon, ctx->vs, 0, 0);
    ID3D11DeviceContext_PSSetShader(ctx->devcon, ctx->ps, 0, 0);

    if (ID3D11Device_CreateInputLayout(ctx->dev, quad_layout, 1, ID3D10Blob_GetBufferPointer(vs), ID3D10Blob_GetBufferSize(vs), &ctx->quad_layout) != 0)
        log_err("create layout failed\n");
    ID3D11DeviceContext_IASetInputLayout(ctx->devcon, ctx->quad_layout);

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.ByteWidth = sizeof(struct Vertex) * 6;      /* Size of vertex buffer */
    bd.Usage = D3D11_USAGE_DEFAULT;                /* write access access by CPU and GPU */
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       /* use as a vertex buffer */

    D3D11_SUBRESOURCE_DATA sd;
    sd.pSysMem = quad;
    sd.SysMemPitch = 0;
    sd.SysMemSlicePitch = 0;

    if (ID3D11Device_CreateBuffer(ctx->dev, &bd, &sd, &ctx->quad) != 0)
        log_err("create buffer failed\n");

    // Create Constant Buffer
    struct Constants
    {
        float2 pos;
        float2 paddingUnused; // color (below) needs to be 16-byte aligned! 
        float4 color;
    };

    ID3D11Buffer* constantBuffer;
    {
        D3D11_BUFFER_DESC constantBufferDesc = {};
        // ByteWidth must be a multiple of 16, per the docs
        constantBufferDesc.ByteWidth = sizeof(Constants) + 0xf & 0xfffffff0;
        constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hResult = d3d11Device->CreateBuffer(&constantBufferDesc, nullptr, &constantBuffer);
        assert(SUCCEEDED(hResult));
    }

    D3D11_SAMPLER_DESC samplerDesc;
    ZeroMemory(&samplerDesc, sizeof samplerDesc);
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.BorderColor[0] = 1.0f;
    samplerDesc.BorderColor[1] = 1.0f;
    samplerDesc.BorderColor[2] = 1.0f;
    samplerDesc.BorderColor[3] = 1.0f;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

    ID3D11SamplerState* samplerState;
    ID3D11Device_CreateSamplerState(ctx->dev, &samplerDesc, &samplerState);

    // Load Image
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;
    unsigned char* testTextureBytes = stbi_load("testTexture.png", &texWidth, &texHeight,
        &texNumChannels, texForceNumChannels);
    assert(testTextureBytes);
    int texBytesPerRow = 4 * texWidth;

    // Create Texture
    D3D11_TEXTURE2D_DESC textureDesc;
    ZeroMemory(&textureDesc, sizeof textureDesc);
    textureDesc.Width = texWidth;
    textureDesc.Height = texHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA textureSubresourceData;
    ZeroMemory(&textureSubresourceData, sizeof textureSubresourceData);
    textureSubresourceData.pSysMem = testTextureBytes;
    textureSubresourceData.SysMemPitch = texBytesPerRow;

    ID3D11Texture2D* texture;
    ID3D11Device_CreateTexture2D(ctx->dev, &textureDesc, &textureSubresourceData, &texture);

    ID3D11ShaderResourceView* textureView;
    ZeroMemory(&textureView, sizeof textureView);
    ID3D11Device_CreateShaderResourceView(ctx->dev, texture, NULL, &textureView);

    iupAttribSet(ih, "ERROR", NULL);
    ID3D10Blob_Release(vs);
    ID3D10Blob_Release(ps);
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

    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = 0.0;
    viewport.MaxDepth = 1.0;

    ID3D11DeviceContext_RSSetViewports(ctx->devcon, 1, &viewport);

    return IUP_DEFAULT;
}

static int CanvasDefaultRedraw_CB(Ihandle* ih, float x, float y)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    log_dbg("Redraw\n");

    ID3D11DeviceContext_OMSetRenderTargets(ctx->devcon, 1, &ctx->backbuffer, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx->devcon, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_IASetInputLayout(ctx->devcon, ctx->quad_layout);

    ID3D11DeviceContext_VSSetShader(ctx->devcon, ctx->vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(ctx->devcon, ctx->ps, NULL, 0);

    UINT stride = sizeof(struct Vertex);
    UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(ctx->devcon, 0, 1, &ctx->quad, &stride, &offset);

    D3D11_MAPPED_SUBRESOURCE mappedSubresource;
    d3d11DeviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
    Constants* constants = (Constants*)(mappedSubresource.pData);
    constants->pos = { 0.25f, 0.3f };
    constants->color = { 0.7f, 0.65f, 0.1f, 1.f };
    d3d11DeviceContext->Unmap(constantBuffer, 0);
    d3d11DeviceContext->VSSetConstantBuffers(0, 1, &constantBuffer);

    ID3D11DeviceContext_PSSetShaderResources(ctx->devcon, 0, 1, &textureView);
    ID3D11DeviceContext_PSSetSamplers(ctx->devcon, 0, 1, &samplerState);

    ID3D11DeviceContext_Draw(ctx->devcon, 6, 0);
    IDXGISwapChain_Present(ctx->swapchain, 0, 0);
}

static int CanvasCreateMethod(Ihandle* ih, void** params)
{
    log_dbg("CanvasCreateMethod\n");
    Context3D* ctx;
    (void)params;

    ctx = mem_alloc(sizeof *ctx);
    iupAttribSet(ih, "_IUP_D3D11CONTEXT", (char*)ctx);

    IupSetCallback(ih, "RESIZE_CB", (Icallback)CanvasDefaultResize_CB);
    IupSetCallback(ih, "ACTION", (Icallback)CanvasDefaultRedraw_CB);

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
    iupAttribSet(ih, "_IUP_D3D11CONTEXT", NULL);
}

int iupdrvGfxInit(void)
{
    return 0;
}

void iupdrvGfxDeInit(void)
{
}

void iupdrvGfxSetTexture(Ihandle* ih, int id, const char* value)
{

}

void iupdrvGfxCanvasInitClass(Iclass* ic)
{
    ic->Create = CanvasCreateMethod;
    ic->Map = CanvasMapMethod;
    ic->UnMap = CanvasUnMapMethod;
    ic->Destroy = CanvasDestroyMethod;
}

int IupGfxIsCurrent(Ihandle* ih)
{
    (void)ih;
    return 1;
}

void IupGfxMakeCurrent(Ihandle* ih)
{
    (void)ih;
}

static FLOAT CLEAR_COLOR[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

void IupGfxSwapBuffers(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_D3D11CONTEXT");
    log_dbg("Swap buffers\n");

    IDXGISwapChain_Present(ctx->swapchain, 0, 0);
}
