#include <windows.h>
#include <stdio.h>
#include <d3d9.h>
#include "fps_limiter.h"
#include "dd.h"
#include "ddsurface.h"
#include "d3d9shader.h"
#include "render_d3d9.h"
#include "utils.h"
#include "wndproc.h"
#include "blt.h"
#include "debug.h"
#include "d3d9types.h"
#include "hook.h"
#include "config.h"
#include <initguid.h>
#include "dsr.h"
#include <d3d12.h>
#include <dxgi1_6.h>

DEFINE_GUID(CLSID_D3D12DSRDeviceFactory, 0x936f7f01, 0x203f, 0x44d3, 0x9e, 0x18, 0x41, 0x98, 0xd2, 0x43, 0xb4, 0xea);
DEFINE_GUID(IID_ID3D12DSRDeviceFactory, 0x936f7f01, 0x203f, 0x44d3, 0x9e, 0x18, 0x41, 0x98, 0xd2, 0x43, 0xb4, 0xeb);
DEFINE_GUID(IID_IDSRDevice, 0x936f7f01, 0x203f, 0x44d3, 0x9e, 0x18, 0x41, 0x98, 0xd2, 0x43, 0xb4, 0xec);
DEFINE_GUID(IID_IDSRSuperResEngine, 0x936f7f01, 0x203f, 0x44d3, 0x9e, 0x18, 0x41, 0x98, 0xd2, 0x43, 0xb4, 0xed);
DEFINE_GUID(IID_IDSRSuperResUpscaler, 0x936f7f01, 0x203f, 0x44d3, 0x9e, 0x18, 0x41, 0x98, 0xd2, 0x43, 0xb4, 0xee);
DEFINE_GUID(IID_IDirect3DDevice9On12, 0xe7fda234, 0xb589, 0x4042, 0x87, 0x34, 0x10, 0x52, 0x33, 0x7, 0x14, 0x58);


#ifdef _DEBUG
#define FAILEDX(stmt) d3d9_check_failed(stmt, #stmt)
#define SUCCEEDEDX(stmt) d3d9_check_succeeded(stmt, #stmt)
static BOOL d3d9_check_failed(HRESULT hr, const char* stmt);
static BOOL d3d9_check_succeeded(HRESULT hr, const char* stmt);
#else
#define FAILEDX(stmt) FAILED(stmt)
#define SUCCEEDEDX(stmt) SUCCEEDED(stmt)
#endif

static BOOL d3d9_create_resources();
static BOOL d3d9_set_states();
static BOOL d3d9_update_vertices(BOOL upscale_hack, BOOL stretch);
static void d3d9_init_dsr();

static D3D9RENDERER g_d3d9;
static IDSRDevice* g_dsr_device;
static IDSRSuperResEngine* g_dsr_engine;
static IDSRSuperResUpscaler* g_dsr_upscaler;
static ID3D12Resource* g_dummy_depth_resource;
static ID3D12Resource* g_dummy_mv_resource;
static IDirect3DDevice9On12* g_d3d9on12_device;

BOOL d3d9_is_available()
{
    LPDIRECT3D9 d3d9 = NULL;

    if ((g_d3d9.hmodule = real_LoadLibraryA("d3d9.dll")))
    {
        if (g_config.d3d9on12)
        {
            D3D9ON12_ARGS args;
            memset(&args, 0, sizeof(args));
            args.Enable9On12 = TRUE;

            IDirect3D9* (WINAPI * d3d_create9on12)(INT, D3D9ON12_ARGS*, UINT) =
                (void*)real_GetProcAddress(g_d3d9.hmodule, "Direct3DCreate9On12");

            if (d3d_create9on12 && (d3d9 = d3d_create9on12(D3D_SDK_VERSION, &args, 1)))
                IDirect3D9_Release(d3d9);
        }

        if (!d3d9)
        {
            IDirect3D9* (WINAPI * d3d_create9)(UINT) =
                (IDirect3D9 * (WINAPI*)(UINT))real_GetProcAddress(g_d3d9.hmodule, "Direct3DCreate9");

            if (d3d_create9 && (d3d9 = d3d_create9(D3D_SDK_VERSION)))
                IDirect3D9_Release(d3d9);
        }
    }

    return d3d9 != NULL;
}

BOOL d3d9_create()
{
    if (g_d3d9.hwnd == g_ddraw.hwnd && d3d9_create_resources() && d3d9_reset(g_config.windowed))
    {
        return TRUE;
    }
    
    d3d9_release();

    if (!g_d3d9.hmodule)
        g_d3d9.hmodule = real_LoadLibraryA("d3d9.dll");

    if (g_d3d9.hmodule)
    {
        LPDIRECT3D9 d3d9on12 = NULL;
        D3D9ON12_ARGS args;
        memset(&args, 0, sizeof(args));
        args.Enable9On12 = TRUE;

        IDirect3D9* (WINAPI * d3d_create9on12)(INT, D3D9ON12_ARGS*, UINT) = NULL;
        IDirect3D9* (WINAPI * d3d_create9)(UINT) = (void*)real_GetProcAddress(g_d3d9.hmodule, "Direct3DCreate9");

        if (g_config.d3d9on12)
        {
            d3d_create9on12 = (void*)real_GetProcAddress(g_d3d9.hmodule, "Direct3DCreate9On12");
        }

        if ((d3d_create9on12 && (d3d9on12 = g_d3d9.instance = d3d_create9on12(D3D_SDK_VERSION, &args, 1))) ||
            (d3d_create9 && (g_d3d9.instance = d3d_create9(D3D_SDK_VERSION))))
        {
#if _DEBUG 
            D3DADAPTER_IDENTIFIER9 ai = {0};
            D3DCAPS9 caps = { 0 };
            HRESULT adapter_hr = IDirect3D9_GetAdapterIdentifier(g_d3d9.instance, D3DADAPTER_DEFAULT, 0, &ai);
            HRESULT devcaps_hr = IDirect3D9_GetDeviceCaps(g_d3d9.instance, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);

            if (SUCCEEDEDX(adapter_hr))
            {
                TRACE("+--Direct3D9-------------------------------------\n");
                TRACE("| D3D9On12:            %s (%p)\n", d3d9on12 != NULL ? "True" : "False", GetModuleHandleA("d3d9on12.dll"));
                TRACE("| VendorId:            0x%x\n", ai.VendorId);
                TRACE("| DeviceId:            0x%x\n", ai.DeviceId);
                TRACE("| Revision:            0x%x\n", ai.Revision);
                TRACE("| SubSysId:            0x%x\n", ai.SubSysId);
                TRACE("| Version:             %hu.%hu.%hu.%hu\n", 
                    HIWORD(ai.DriverVersion.HighPart), 
                    LOWORD(ai.DriverVersion.HighPart), 
                    HIWORD(ai.DriverVersion.LowPart), 
                    LOWORD(ai.DriverVersion.LowPart));

                TRACE("| Driver:              %s\n", ai.Driver);
                TRACE("| Description:         %s\n", ai.Description);

                if (SUCCEEDEDX(devcaps_hr))
                {
                    TRACE("| MaxTextureWidth:     %d\n", caps.MaxTextureWidth);
                    TRACE("| MaxTextureHeight:    %d\n", caps.MaxTextureHeight);

                    TRACE("| VertexShaderVersion: %d.%d\n",
                        (caps.VertexShaderVersion >> 8) & 0xFF,
                        caps.VertexShaderVersion & 0xFF);

                    TRACE("| PixelShaderVersion:  %d.%d\n",
                        (caps.PixelShaderVersion >> 8) & 0xFF,
                        caps.PixelShaderVersion & 0xFF);
                }
                TRACE("+------------------------------------------------\n");
            }
#endif
            g_d3d9.hwnd = g_ddraw.hwnd;

            memset(&g_d3d9.params, 0, sizeof(g_d3d9.params));

            g_d3d9.params.Windowed = g_config.windowed || g_config.nonexclusive;
            g_d3d9.params.SwapEffect = D3DSWAPEFFECT_DISCARD;
            g_d3d9.params.hDeviceWindow = g_ddraw.hwnd;
            g_d3d9.params.PresentationInterval = g_config.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
            g_d3d9.params.BackBufferWidth = g_d3d9.params.Windowed ? 0 : g_ddraw.render.width;
            g_d3d9.params.BackBufferHeight = g_d3d9.params.Windowed ? 0 : g_ddraw.render.height;
            g_d3d9.params.FullScreen_RefreshRateInHz = g_d3d9.params.Windowed ? 0 : g_config.refresh_rate;
            g_d3d9.params.BackBufferFormat = g_ddraw.mode.dmBitsPerPel == 16 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
            g_d3d9.params.BackBufferCount = 1;

            DWORD behavior_flags[] = {
                D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE,
                D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE,
                D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
                D3DCREATE_MULTITHREADED | D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
                D3DCREATE_MULTITHREADED | D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
            };

            for (int i = 0; i < sizeof(behavior_flags) / sizeof(behavior_flags[0]); i++)
            {
                if (SUCCEEDEDX(
                    IDirect3D9_CreateDevice(
                        g_d3d9.instance,
                        D3DADAPTER_DEFAULT,
                        D3DDEVTYPE_HAL,
                        g_ddraw.hwnd,
                        behavior_flags[i],
                        &g_d3d9.params,
                        &g_d3d9.device)))
                {
                    if (g_config.d3d9on12)
                    {
                        g_d3d9.device->lpVtbl->QueryInterface(g_d3d9.device, &IID_IDirect3DDevice9On12, (void**)&g_d3d9on12_device);
                    }

                    if (g_config.superresolution)
                    {
                        d3d9_init_dsr();
                    }
                    return g_d3d9.device && d3d9_create_resources() && d3d9_set_states();
                }
            }
        }
    }

    return FALSE;
}

static void d3d9_init_dsr()
{
    if (!g_d3d9.device)
        return;

    if (!g_d3d9on12_device)
        return;

    ID3D12Device* d3d12_device = NULL;
    if (FAILED(g_d3d9on12_device->lpVtbl->GetD3D12Device(g_d3d9on12_device, &IID_ID3D12Device, (void**)&d3d12_device)))
    {
        return;
    }

    ID3D12DSRDeviceFactory* dsr_factory = NULL;

    HRESULT(WINAPI * d3d12_get_interface)(REFCLSID, REFIID, void**) =
        (void*)real_GetProcAddress(GetModuleHandleA("d3d12.dll"), "D3D12GetInterface");

    if (!d3d12_get_interface || FAILED(d3d12_get_interface(&CLSID_D3D12DSRDeviceFactory, &IID_ID3D12DSRDeviceFactory, (void**)&dsr_factory)))
    {
        d3d12_device->lpVtbl->Release(d3d12_device);
        return;
    }

    if (FAILED(dsr_factory->lpVtbl->CreateDSRDevice(dsr_factory, d3d12_device, &IID_IDSRDevice, (void**)&g_dsr_device)))
    {
        dsr_factory->lpVtbl->Release(dsr_factory);
        d3d12_device->lpVtbl->Release(d3d12_device);
        return;
    }

    UINT variant_count = g_dsr_device->lpVtbl->GetNumSuperResVariants(g_dsr_device);
    TRACE("DirectSR: Found %d super resolution variants\n", variant_count);

    if (variant_count == 0)
    {
        dsr_factory->lpVtbl->Release(dsr_factory);
        d3d12_device->lpVtbl->Release(d3d12_device);
        return;
    }

    DSR_SUPERRES_VARIANT_DESC variant_desc;
    if (FAILED(g_dsr_device->lpVtbl->GetSuperResVariantDesc(g_dsr_device, 0, &variant_desc)))
    {
        dsr_factory->lpVtbl->Release(dsr_factory);
        d3d12_device->lpVtbl->Release(d3d12_device);
        return;
    }

    TRACE("DirectSR: Using variant %s\n", variant_desc.VariantName);

    DSR_SUPERRES_CREATE_ENGINE_PARAMETERS engine_params = { 0 };
    engine_params.VariantId = variant_desc.VariantId;
    engine_params.TargetFormat = g_d3d9.params.BackBufferFormat;
    engine_params.SourceColorFormat = g_ddraw.bpp == 16 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
    engine_params.SourceDepthFormat = DXGI_FORMAT_D32_FLOAT;
    engine_params.Flags = DSR_SUPERRES_CREATE_ENGINE_FLAG_NONE;
    engine_params.TargetSize.Width = g_ddraw.render.width;
    engine_params.TargetSize.Height = g_ddraw.render.height;
    engine_params.MaxSourceSize.Width = g_ddraw.width;
    engine_params.MaxSourceSize.Height = g_ddraw.height;

    if (FAILED(g_dsr_device->lpVtbl->CreateSuperResEngine(g_dsr_device, &engine_params, &IID_IDSRSuperResEngine, (void**)&g_dsr_engine)))
    {
        dsr_factory->lpVtbl->Release(dsr_factory);
        d3d12_device->lpVtbl->Release(d3d12_device);
        return;
    }

    ID3D12CommandQueue* command_queue = NULL;
    D3D12_COMMAND_QUEUE_DESC queue_desc = { 0 };
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(d3d12_device->lpVtbl->CreateCommandQueue(d3d12_device, &queue_desc, &IID_ID3D12CommandQueue, (void**)&command_queue)))
    {
        dsr_factory->lpVtbl->Release(dsr_factory);
        d3d12_device->lpVtbl->Release(d3d12_device);
        return;
    }

    if (FAILED(g_dsr_engine->lpVtbl->CreateUpscaler(g_dsr_engine, command_queue, &IID_IDSRSuperResUpscaler, (void**)&g_dsr_upscaler)))
    {
        command_queue->lpVtbl->Release(command_queue);
        dsr_factory->lpVtbl->Release(dsr_factory);
        d3d12_device->lpVtbl->Release(d3d12_device);
        return;
    }

    D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC resource_desc = {
        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        0,
        g_ddraw.width,
        g_ddraw.height,
        1,
        1,
        DXGI_FORMAT_D32_FLOAT,
        {1, 0},
        D3D12_TEXTURE_LAYOUT_UNKNOWN,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    };

    if (SUCCEEDED(d3d12_device->lpVtbl->CreateCommittedResource(
        d3d12_device,
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        NULL,
        &IID_ID3D12Resource,
        (void**)&g_dummy_depth_resource)))
    {
        resource_desc.Format = DXGI_FORMAT_R16G16_FLOAT;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        d3d12_device->lpVtbl->CreateCommittedResource(
            d3d12_device,
            &heap_props,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            NULL,
            &IID_ID3D12Resource,
            (void**)&g_dummy_mv_resource);
    }

    command_queue->lpVtbl->Release(command_queue);
    dsr_factory->lpVtbl->Release(dsr_factory);
    d3d12_device->lpVtbl->Release(d3d12_device);
}

#ifdef _DEBUG
static BOOL d3d9_check_failed(HRESULT hr, const char* stmt)
{
    if (FAILED(hr))
    {
        TRACE("Direct3D9 error %s [%08x] (%s)\n", dbg_d3d9_hr_to_str(hr), hr, stmt);
        return TRUE;
    }

    return FALSE;
}

static BOOL d3d9_check_succeeded(HRESULT hr, const char* stmt)
{
    if (!SUCCEEDED(hr))
    {
        TRACE("Direct3D9 error %s [%08x] (%s)\n", dbg_d3d9_hr_to_str(hr), hr, stmt);

        return FALSE;
    }

    return TRUE;
}
#endif

BOOL d3d9_on_device_lost()
{
    if (g_d3d9.device && IDirect3DDevice9_TestCooperativeLevel(g_d3d9.device) == D3DERR_DEVICENOTRESET)
    {
        return d3d9_reset(g_config.windowed);
    }

    return FALSE;
}

BOOL d3d9_reset(BOOL windowed)
{
    g_d3d9.params.Windowed = windowed || g_config.nonexclusive;
    g_d3d9.params.BackBufferWidth = g_d3d9.params.Windowed ? 0 : g_ddraw.render.width;
    g_d3d9.params.BackBufferHeight = g_d3d9.params.Windowed ? 0 : g_ddraw.render.height;
    g_d3d9.params.FullScreen_RefreshRateInHz = g_d3d9.params.Windowed ? 0 : g_config.refresh_rate;
    g_d3d9.params.BackBufferFormat = g_ddraw.mode.dmBitsPerPel == 16 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;

    if (g_d3d9.device && SUCCEEDEDX(IDirect3DDevice9_Reset(g_d3d9.device, &g_d3d9.params)))
    {
        BOOL result = d3d9_set_states();

        if (result)
        {
            InterlockedExchange(&g_ddraw.render.palette_updated, TRUE);
            InterlockedExchange(&g_ddraw.render.surface_updated, TRUE);
            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
        }

        return result;
    }

    return FALSE;
}

BOOL d3d9_release_resources()
{
    if (g_d3d9.pixel_shader)
    {
        IDirect3DPixelShader9_Release(g_d3d9.pixel_shader);
        g_d3d9.pixel_shader = NULL;
    }

    if (g_d3d9.pixel_shader_upscale)
    {
        IDirect3DPixelShader9_Release(g_d3d9.pixel_shader_upscale);
        g_d3d9.pixel_shader_upscale = NULL;
    }

    for (int i = 0; i < D3D9_TEXTURE_COUNT; i++)
    {
        if (g_d3d9.surface_tex[i])
        {
            IDirect3DTexture9_Release(g_d3d9.surface_tex[i]);
            g_d3d9.surface_tex[i] = NULL;
        }

        if (g_d3d9.palette_tex[i])
        {
            IDirect3DTexture9_Release(g_d3d9.palette_tex[i]);
            g_d3d9.palette_tex[i] = NULL;
        }
    }

    if (g_d3d9.vertex_buf)
    {
        IDirect3DVertexBuffer9_Release(g_d3d9.vertex_buf);
        g_d3d9.vertex_buf = NULL;
    }

    return TRUE;
}

BOOL d3d9_release()
{
    d3d9_release_resources();

    if (g_d3d9on12_device)
    {
        g_d3d9on12_device->lpVtbl->Release(g_d3d9on12_device);
        g_d3d9on12_device = NULL;
    }

    if (g_dummy_depth_resource)
    {
        g_dummy_depth_resource->lpVtbl->Release(g_dummy_depth_resource);
        g_dummy_depth_resource = NULL;
    }

    if (g_dummy_mv_resource)
    {
        g_dummy_mv_resource->lpVtbl->Release(g_dummy_mv_resource);
        g_dummy_mv_resource = NULL;
    }

    if (g_dsr_upscaler)
    {
        g_dsr_upscaler->lpVtbl->Release(g_dsr_upscaler);
        g_dsr_upscaler = NULL;
    }

    if (g_dsr_engine)
    {
        g_dsr_engine->lpVtbl->Release(g_dsr_engine);
        g_dsr_engine = NULL;
    }

    if (g_dsr_device)
    {
        g_dsr_device->lpVtbl->Release(g_dsr_device);
        g_dsr_device = NULL;
    }

    if (g_d3d9.device)
    {
        while (IDirect3DDevice9_Release(g_d3d9.device));
        g_d3d9.device = NULL;
    }

    if (g_d3d9.instance)
    {
        while (IDirect3D9_Release(g_d3d9.instance));
        g_d3d9.instance = NULL;
    }

    return TRUE;
}

static BOOL d3d9_create_resources()
{
    if (!g_d3d9.device)
        return FALSE;

    d3d9_release_resources();

    BOOL err = FALSE;

    int width = g_ddraw.width;
    int height = g_ddraw.height;

    g_d3d9.tex_width =
        width <= 1024 ? 1024 : width <= 2048 ? 2048 : width <= 4096 ? 4096 : width;

    g_d3d9.tex_height =
        height <= g_d3d9.tex_width ? g_d3d9.tex_width : height <= 2048 ? 2048 : height <= 4096 ? 4096 : height;

    g_d3d9.tex_width = g_d3d9.tex_width > g_d3d9.tex_height ? g_d3d9.tex_width : g_d3d9.tex_height;

    g_d3d9.scale_w = (float)width / g_d3d9.tex_width;;
    g_d3d9.scale_h = (float)height / g_d3d9.tex_height;

    err = err || FAILEDX(
        IDirect3DDevice9_CreateVertexBuffer(
            g_d3d9.device,
            sizeof(CUSTOMVERTEX) * 4, 0,
            D3DFVF_XYZRHW | D3DFVF_TEX1,
            D3DPOOL_MANAGED,
            &g_d3d9.vertex_buf,
            NULL));

    err = err || !d3d9_update_vertices(InterlockedExchangeAdd(&g_ddraw.upscale_hack_active, 0), TRUE);

    for (int i = 0; i < D3D9_TEXTURE_COUNT; i++)
    {
        if (g_ddraw.bpp == 16 && g_config.rgb555)
        {
            BOOL error = FAILEDX(
                IDirect3DDevice9_CreateTexture(
                    g_d3d9.device,
                    g_d3d9.tex_width,
                    g_d3d9.tex_height,
                    1,
                    0,
                    D3DFMT_X1R5G5B5,
                    D3DPOOL_MANAGED,
                    &g_d3d9.surface_tex[i],
                    0));
            
            if (error)
            {
                err = err || FAILEDX(
                    IDirect3DDevice9_CreateTexture(
                        g_d3d9.device,
                        g_d3d9.tex_width,
                        g_d3d9.tex_height,
                        1,
                        0,
                        D3DFMT_A1R5G5B5,
                        D3DPOOL_MANAGED,
                        &g_d3d9.surface_tex[i],
                        0));
            }
        }
        else if (g_ddraw.bpp == 32)
        {
            BOOL error = FAILEDX(
                IDirect3DDevice9_CreateTexture(
                    g_d3d9.device,
                    g_d3d9.tex_width,
                    g_d3d9.tex_height,
                    1,
                    0,
                    D3DFMT_X8R8G8B8,
                    D3DPOOL_MANAGED,
                    &g_d3d9.surface_tex[i],
                    0));

            if (error)
            {
                err = err || FAILEDX(
                    IDirect3DDevice9_CreateTexture(
                        g_d3d9.device,
                        g_d3d9.tex_width,
                        g_d3d9.tex_height,
                        1,
                        0,
                        D3DFMT_A8R8G8B8,
                        D3DPOOL_MANAGED,
                        &g_d3d9.surface_tex[i],
                        0));
            }
        }
        else
        {
            err = err || FAILEDX(
                IDirect3DDevice9_CreateTexture(
                    g_d3d9.device,
                    g_d3d9.tex_width,
                    g_d3d9.tex_height,
                    1,
                    0,
                    g_ddraw.bpp == 16 ? D3DFMT_R5G6B5 : D3DFMT_L8,
                    D3DPOOL_MANAGED,
                    &g_d3d9.surface_tex[i],
                    0));
        }

        err = err || !g_d3d9.surface_tex[i];

        if (g_ddraw.bpp == 8)
        {
            BOOL error = FAILEDX(
                IDirect3DDevice9_CreateTexture(
                    g_d3d9.device,
                    256,
                    256,
                    1,
                    0,
                    D3DFMT_X8R8G8B8,
                    D3DPOOL_MANAGED,
                    &g_d3d9.palette_tex[i],
                    0));

            if (error)
            {
                err = err || FAILEDX(
                    IDirect3DDevice9_CreateTexture(
                        g_d3d9.device,
                        256,
                        256,
                        1,
                        0,
                        D3DFMT_A8R8G8B8,
                        D3DPOOL_MANAGED,
                        &g_d3d9.palette_tex[i],
                        0));
            }

            err = err || !g_d3d9.palette_tex[i];
        }
    }

    if (g_ddraw.bpp == 8)
    {
        err = err || FAILEDX(
            IDirect3DDevice9_CreatePixelShader(g_d3d9.device, (DWORD*)D3D9_PALETTE_SHADER, &g_d3d9.pixel_shader));

        IDirect3DDevice9_CreatePixelShader(
            g_d3d9.device, 
            (DWORD*)D3D9_PALETTE_SHADER_BILINEAR, 
            &g_d3d9.pixel_shader_upscale);
    }
    else
    {
        if (g_config.d3d9_filter == FILTER_LANCZOS)
        {
            BOOL error = FAILEDX(
                IDirect3DDevice9_CreatePixelShader(
                    g_d3d9.device,
                    (DWORD*)D3D9_LANCZOS2_SHADER,
                    &g_d3d9.pixel_shader_upscale));

            if (error || !g_d3d9.pixel_shader_upscale)
            {
                g_config.d3d9_filter = FILTER_CUBIC;
            }
        }

        if (g_config.d3d9_filter == FILTER_CUBIC)
        {
            IDirect3DDevice9_CreatePixelShader(
                g_d3d9.device,
                (DWORD*)D3D9_CATMULL_ROM_SHADER,
                &g_d3d9.pixel_shader_upscale);
        }
    }

    return g_d3d9.vertex_buf && (g_d3d9.pixel_shader || g_ddraw.bpp == 16 || g_ddraw.bpp == 32) && !err;
}

static BOOL d3d9_set_states()
{
    BOOL err = FALSE;

    err = err || FAILEDX(IDirect3DDevice9_SetFVF(g_d3d9.device, D3DFVF_XYZRHW | D3DFVF_TEX1));
    err = err || FAILEDX(IDirect3DDevice9_SetStreamSource(g_d3d9.device, 0, g_d3d9.vertex_buf, 0, sizeof(CUSTOMVERTEX)));
    err = err || FAILEDX(IDirect3DDevice9_SetTexture(g_d3d9.device, 0, (IDirect3DBaseTexture9*)g_d3d9.surface_tex[0]));

    if (g_ddraw.bpp == 8)
    {
        err = err || FAILEDX(IDirect3DDevice9_SetTexture(g_d3d9.device, 1, (IDirect3DBaseTexture9*)g_d3d9.palette_tex[0]));
        
        BOOL bilinear =
            g_config.d3d9_filter &&
            g_d3d9.pixel_shader_upscale &&
            (g_ddraw.render.viewport.width != g_ddraw.width || g_ddraw.render.viewport.height != g_ddraw.height || g_config.vhack);

        err = err || FAILEDX(
            IDirect3DDevice9_SetPixelShader(
                g_d3d9.device, 
                bilinear ? g_d3d9.pixel_shader_upscale : g_d3d9.pixel_shader));

        if (bilinear)
        {
            float texture_size[4] = { (float)g_d3d9.tex_width, (float)g_d3d9.tex_height, 0, 0 };
            err = err || FAILEDX(IDirect3DDevice9_SetPixelShaderConstantF(g_d3d9.device, 0, texture_size, 1));
        }
    }
    else
    {
        if (g_config.d3d9_filter)
        {
            if (g_config.d3d9_filter == FILTER_LANCZOS &&
                g_d3d9.pixel_shader_upscale &&
                (g_ddraw.render.viewport.width != g_ddraw.width || 
                    g_ddraw.render.viewport.height != g_ddraw.height) &&
                SUCCEEDEDX(IDirect3DDevice9_SetPixelShader(g_d3d9.device, g_d3d9.pixel_shader_upscale)))
            {
                float texture_size[4] = { (float)g_d3d9.tex_width, (float)g_d3d9.tex_height, 0, 0 };
                err = err || FAILEDX(IDirect3DDevice9_SetPixelShaderConstantF(g_d3d9.device, 0, texture_size, 1));
            }
            else if (
                SUCCEEDEDX(IDirect3DDevice9_SetSamplerState(g_d3d9.device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)) &&
                SUCCEEDEDX(IDirect3DDevice9_SetSamplerState(g_d3d9.device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)) &&
                g_config.d3d9_filter == FILTER_CUBIC &&
                g_d3d9.pixel_shader_upscale &&
                (g_ddraw.render.viewport.width != g_ddraw.width || 
                    g_ddraw.render.viewport.height != g_ddraw.height) &&
                SUCCEEDEDX(IDirect3DDevice9_SetPixelShader(g_d3d9.device, g_d3d9.pixel_shader_upscale)))
            {
                float texture_size[4] = { (float)g_d3d9.tex_width, (float)g_d3d9.tex_height, 0, 0 };
                err = err || FAILEDX(IDirect3DDevice9_SetPixelShaderConstantF(g_d3d9.device, 0, texture_size, 1));
            }
        }
    }

    /*
    D3DVIEWPORT9 view_data = {
        g_ddraw.render.viewport.x,
        g_ddraw.render.viewport.y,
        g_ddraw.render.viewport.width,
        g_ddraw.render.viewport.height,
        0.0f,
        1.0f };

    err = err || FAILEDX(IDirect3DDevice9_SetViewport(g_d3d9.device, &view_data));
    */
    return !err;
}

static BOOL d3d9_update_vertices(BOOL upscale_hack, BOOL stretch)
{
    float vp_x = stretch ? (float)g_ddraw.render.viewport.x : 0.0f;
    float vp_y = stretch ? (float)g_ddraw.render.viewport.y : 0.0f;

    float vp_w = stretch ? (float)(g_ddraw.render.viewport.width + g_ddraw.render.viewport.x) : (float)g_ddraw.width;
    float vp_h = stretch ? (float)(g_ddraw.render.viewport.height + g_ddraw.render.viewport.y) : (float)g_ddraw.height;

    float s_h = upscale_hack ? g_d3d9.scale_h * ((float)g_ddraw.upscale_hack_height / g_ddraw.height) : g_d3d9.scale_h;
    float s_w = upscale_hack ? g_d3d9.scale_w * ((float)g_ddraw.upscale_hack_width / g_ddraw.width) : g_d3d9.scale_w;

    CUSTOMVERTEX vertices[] =
    {
        { vp_x - 0.5f, vp_h - 0.5f, 0.0f, 1.0f, 0.0f, s_h },
        { vp_x - 0.5f, vp_y - 0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
        { vp_w - 0.5f, vp_h - 0.5f, 0.0f, 1.0f, s_w,  s_h },
        { vp_w - 0.5f, vp_y - 0.5f, 0.0f, 1.0f, s_w,  0.0f }
    };

    void* data;
    if (g_d3d9.vertex_buf && SUCCEEDEDX(IDirect3DVertexBuffer9_Lock(g_d3d9.vertex_buf, 0, 0, (void**)&data, 0)))
    {
        memcpy(data, vertices, sizeof(vertices));

        IDirect3DVertexBuffer9_Unlock(g_d3d9.vertex_buf);
        return TRUE;
    }

    return FALSE;
}

DWORD WINAPI d3d9_render_main(void)
{
    Sleep(250);

    fpsl_init();

    BOOL needs_update = FALSE;

    DWORD timeout = g_config.minfps > 0 ? g_ddraw.minfps_tick_len : INFINITE;

    while (g_ddraw.render.run &&
        (g_config.minfps < 0 || WaitForSingleObject(g_ddraw.render.sem, timeout) != WAIT_FAILED) &&
        g_ddraw.render.run)
    {
#if _DEBUG
        dbg_draw_frame_info_start();
#endif

        static int tex_index = 0, pal_index = 0;

        fpsl_frame_start();

        EnterCriticalSection(&g_ddraw.cs);

        if (g_ddraw.primary && 
            g_ddraw.primary->bpp == g_ddraw.bpp &&
            g_ddraw.primary->width == g_ddraw.width &&
            g_ddraw.primary->height == g_ddraw.height &&
            (g_ddraw.bpp == 16 || g_ddraw.bpp == 32 || g_ddraw.primary->palette))
        {
            if (g_config.lock_surfaces)
                EnterCriticalSection(&g_ddraw.primary->cs);

            if (g_config.vhack)
            {
                if (util_detect_low_res_screen())
                {
                    if (!InterlockedExchange(&g_ddraw.upscale_hack_active, TRUE))
                        d3d9_update_vertices(TRUE, TRUE);
                }
                else
                {
                    if (InterlockedExchange(&g_ddraw.upscale_hack_active, FALSE))
                        d3d9_update_vertices(FALSE, TRUE);
                }
            }

            D3DLOCKED_RECT lock_rc;

            if (InterlockedExchange(&g_ddraw.render.surface_updated, FALSE) || g_config.minfps == -2)
            {
                if (++tex_index >= D3D9_TEXTURE_COUNT)
                    tex_index = 0;

                RECT rc = { 0, 0, g_ddraw.width, g_ddraw.height };

                if (SUCCEEDEDX(IDirect3DDevice9_SetTexture(g_d3d9.device, 0, (IDirect3DBaseTexture9*)g_d3d9.surface_tex[tex_index])) &&
                    SUCCEEDEDX(IDirect3DTexture9_LockRect(g_d3d9.surface_tex[tex_index], 0, &lock_rc, &rc, 0)))
                {
                    blt_clean(
                        lock_rc.pBits,
                        0,
                        0,
                        g_ddraw.primary->width,
                        g_ddraw.primary->height,
                        lock_rc.Pitch,
                        g_ddraw.primary->surface,
                        0,
                        0,
                        g_ddraw.primary->pitch,
                        g_ddraw.primary->bpp);

                    IDirect3DTexture9_UnlockRect(g_d3d9.surface_tex[tex_index], 0);
                }
            }

            if (g_ddraw.bpp == 8 &&
                (InterlockedExchange(&g_ddraw.render.palette_updated, FALSE) || g_config.minfps == -2))
            {
                if (++pal_index >= D3D9_TEXTURE_COUNT)
                    pal_index = 0;

                RECT rc = { 0,0,256,1 };

                if (SUCCEEDEDX(IDirect3DDevice9_SetTexture(g_d3d9.device, 1, (IDirect3DBaseTexture9*)g_d3d9.palette_tex[pal_index])) &&
                    SUCCEEDEDX(IDirect3DTexture9_LockRect(g_d3d9.palette_tex[pal_index], 0, &lock_rc, &rc, 0)))
                {
                    memcpy(lock_rc.pBits, g_ddraw.primary->palette->data_rgb, 256 * sizeof(int));

                    IDirect3DTexture9_UnlockRect(g_d3d9.palette_tex[pal_index], 0);
                }
            }

            if (g_config.fixchilds)
            {
                g_ddraw.child_window_exists = FALSE;
                EnumChildWindows(g_ddraw.hwnd, util_enum_child_proc, (LPARAM)g_ddraw.primary);

                if (g_ddraw.render.width != g_ddraw.width || g_ddraw.render.height != g_ddraw.height)
                {
                    if (g_ddraw.child_window_exists)
                    {
                        IDirect3DDevice9_Clear(g_d3d9.device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

                        if (!needs_update && d3d9_update_vertices(FALSE, FALSE))
                            needs_update = TRUE;
                    }
                    else if (needs_update)
                    {
                        if (d3d9_update_vertices(FALSE, TRUE))
                            needs_update = FALSE;
                    }
                }
            }

            if (g_config.lock_surfaces)
                LeaveCriticalSection(&g_ddraw.primary->cs);
        }

        LeaveCriticalSection(&g_ddraw.cs);

        if (g_ddraw.render.viewport.x != 0 || g_ddraw.render.viewport.y != 0)
        {
            IDirect3DDevice9_Clear(g_d3d9.device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        }

        IDirect3DDevice9_BeginScene(g_d3d9.device);
        IDirect3DDevice9_DrawPrimitive(g_d3d9.device, D3DPT_TRIANGLESTRIP, 0, 2);
        IDirect3DDevice9_EndScene(g_d3d9.device);

        if (g_dsr_upscaler)
        {
            ID3D12Resource* src_res = NULL;
            if (SUCCEEDED(g_d3d9on12_device->lpVtbl->GetD3D12Resource(g_d3d9on12_device, (IDirect3DResource9*)g_d3d9.surface_tex[tex_index], &IID_ID3D12Resource, (void**)&src_res)))
            {
                IDirect3DSurface9* back_buffer = NULL;
                if (SUCCEEDED(g_d3d9.device->lpVtbl->GetBackBuffer(g_d3d9.device, 0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)))
                {
                    ID3D12Resource* dst_res = NULL;
                    if (SUCCEEDED(g_d3d9on12_device->lpVtbl->GetD3D12Resource(g_d3d9on12_device, (IDirect3DResource9*)back_buffer, &IID_ID3D12Resource, (void**)&dst_res)))
                    {
                        DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS params = { 0 };
                        params.pSourceColorTexture = src_res;
                        params.pTargetTexture = dst_res;
                        params.pSourceDepthTexture = g_dummy_depth_resource;
                        params.pMotionVectorsTexture = g_dummy_mv_resource;

                        g_dsr_upscaler->lpVtbl->Execute(g_dsr_upscaler, &params, 0, DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_NONE);

                        dst_res->lpVtbl->Release(dst_res);
                    }
                    back_buffer->lpVtbl->Release(back_buffer);
                }
                src_res->lpVtbl->Release(src_res);
            }
        }

        if (g_ddraw.bnet_active)
        {
            IDirect3DDevice9_Clear(g_d3d9.device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        }

        if (FAILED(IDirect3DDevice9_Present(g_d3d9.device, NULL, NULL, NULL, NULL)))
        {
            DWORD_PTR result;
            SendMessageTimeout(g_ddraw.hwnd, WM_D3D9DEVICELOST, 0, 0, 0, 1000, &result);

            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
            Sleep(50);
        }
        else
        {
            /* Force redraw for GDI games (ClueFinders) */
            if (!g_ddraw.primary)
            {
                RedrawWindow(g_ddraw.hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
            }
        }

        if (!g_ddraw.render.run)
            break;

#if _DEBUG
        dbg_draw_frame_info_end();
#endif

        fpsl_frame_end();
    }

    if (g_config.vhack)
        InterlockedExchange(&g_ddraw.upscale_hack_active, FALSE);

    return 0;
}
