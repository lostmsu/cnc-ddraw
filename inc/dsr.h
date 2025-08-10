#ifndef DSR_H
#define DSR_H

#include <d3d12.h>
#include <dxgi1_6.h>

typedef struct DSR_SIZE
{
    UINT Width;
    UINT Height;
} DSR_SIZE;

typedef enum DSR_SUPERRES_CREATE_ENGINE_FLAGS
{
    DSR_SUPERRES_CREATE_ENGINE_FLAG_NONE = 0x0,
    DSR_SUPERRES_CREATE_ENGINE_FLAG_MOTION_VECTORS_USE_TARGET_DIMENSIONS = 0x1,
    DSR_SUPERRES_CREATE_ENGINE_FLAG_AUTO_EXPOSURE = 0x2,
    DSR_SUPERRES_CREATE_ENGINE_FLAG_ALLOW_DRS = 0x4,
    DSR_SUPERRES_CREATE_ENGINE_FLAG_MOTION_VECTORS_USE_JITTER_OFFSETS = 0x8,
    DSR_SUPERRES_CREATE_ENGINE_FLAG_ALLOW_SUBRECT_OUTPUT = 0x10,
    DSR_SUPERRES_CREATE_ENGINE_FLAG_LINEAR_DEPTH = 0x20,
    DSR_SUPERRES_CREATE_ENGINE_FLAG_ENABLE_SHARPENING = 0x40,
    DSR_SUPERRES_CREATE_ENGINE_FLAG_FORCE_LDR_COLORS = 0x80,
} DSR_SUPERRES_CREATE_ENGINE_FLAGS;

typedef struct DSR_SUPERRES_CREATE_ENGINE_PARAMETERS
{
    GUID VariantId;
    DXGI_FORMAT TargetFormat;
    DXGI_FORMAT SourceColorFormat;
    DXGI_FORMAT SourceDepthFormat;
    DXGI_FORMAT ExposureScaleFormat;
    DSR_SUPERRES_CREATE_ENGINE_FLAGS Flags;
    DSR_SIZE MaxSourceSize;
    DSR_SIZE TargetSize;
} DSR_SUPERRES_CREATE_ENGINE_PARAMETERS;

typedef struct DSR_SUPERRES_SOURCE_SETTINGS
{
    DSR_SIZE OptimalSize;
    DSR_SIZE MinDynamicSize;
    DSR_SIZE MaxDynamicSize;
    DXGI_FORMAT OptimalColorFormat;
    DXGI_FORMAT OptimalDepthFormat;
} DSR_SUPERRES_SOURCE_SETTINGS;

typedef enum DSR_OPTIMIZATION_TYPE
{
    DSR_OPTIMIZATION_TYPE_BALANCED,
    DSR_OPTIMIZATION_TYPE_HIGH_QUALITY,
    DSR_OPTIMIZATION_TYPE_MAX_QUALITY,
    DSR_OPTIMIZATION_TYPE_HIGH_PERFORMANCE,
    DSR_OPTIMIZATION_TYPE_MAX_PERFORMANCE,
    DSR_OPTIMIZATION_TYPE_POWER_SAVING,
    DSR_OPTIMIZATION_TYPE_MAX_POWER_SAVING,
    DSR_NUM_OPTIMIZATION_TYPES,
} DSR_OPTIMIZATION_TYPE;

typedef enum DSR_SUPERRES_VARIANT_FLAGS
{
    DSR_SUPERRES_VARIANT_FLAG_NONE = 0x0,
    DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_EXPOSURE_SCALE_TEXTURE = 0x1,
    DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_IGNORE_HISTORY_MASK = 0x2,
    DSR_SUPERRES_VARIANT_FLAG_NATIVE = 0x4,
    DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_REACTIVE_MASK = 0x8,
    DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_SHARPNESS = 0x10,
    DSR_SUPERRES_VARIANT_FLAG_DISALLOWS_REGION_OFFSETS = 0x20,
} DSR_SUPERRES_VARIANT_FLAGS;

typedef struct DSR_SUPERRES_VARIANT_DESC
{
    GUID VariantId;
    CHAR VariantName[128];
    DSR_SUPERRES_VARIANT_FLAGS Flags;
    DSR_OPTIMIZATION_TYPE OptimizationRankings[DSR_NUM_OPTIMIZATION_TYPES];
    DXGI_FORMAT OptimalTargetFormat;
} DSR_SUPERRES_VARIANT_DESC;

typedef interface IDSRDevice IDSRDevice;
typedef interface IDSRDeviceVtbl
{
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE* QueryInterface)(
        IDSRDevice* This,
        REFIID riid,
        _COM_Outptr_  void** ppvObject);

    ULONG(STDMETHODCALLTYPE* AddRef)(
        IDSRDevice* This);

    ULONG(STDMETHODCALLTYPE* Release)(
        IDSRDevice* This);

    UINT(STDMETHODCALLTYPE* GetNumSuperResVariants)(
        IDSRDevice* This);

    HRESULT(STDMETHODCALLTYPE* GetSuperResVariantDesc)(
        IDSRDevice* This,
        UINT VariantIndex,
        _Out_ DSR_SUPERRES_VARIANT_DESC* pVariantDesc);

    HRESULT(STDMETHODCALLTYPE* QuerySuperResSourceSettings)(
        IDSRDevice* This,
        UINT VariantIndex,
        DSR_SIZE TargetSize,
        DXGI_FORMAT TargetFormat,
        DSR_OPTIMIZATION_TYPE OptimizationType,
        DSR_SUPERRES_CREATE_ENGINE_FLAGS CreateFlags,
        _Out_ DSR_SUPERRES_SOURCE_SETTINGS* pSourceSettings);

    HRESULT(STDMETHODCALLTYPE* CreateSuperResEngine)(
        IDSRDevice* This,
        _In_ const DSR_SUPERRES_CREATE_ENGINE_PARAMETERS* pDesc,
        _In_ REFIID iid,
        _COM_Outptr_ void** ppEngine);

    END_INTERFACE
} IDSRDeviceVtbl;

interface IDSRDevice
{
    CONST_VTBL struct IDSRDeviceVtbl* lpVtbl;
};


typedef interface ID3D12DSRDeviceFactory ID3D12DSRDeviceFactory;
typedef interface ID3D12DSRDeviceFactoryVtbl
{
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE* QueryInterface)(
        ID3D12DSRDeviceFactory* This,
        REFIID riid,
        _COM_Outptr_  void** ppvObject);

    ULONG(STDMETHODCALLTYPE* AddRef)(
        ID3D12DSRDeviceFactory* This);

    ULONG(STDMETHODCALLTYPE* Release)(
        ID3D12DSRDeviceFactory* This);

    HRESULT(STDMETHODCALLTYPE* CreateDSRDevice)(
        ID3D12DSRDeviceFactory* This,
        _In_ ID3D12Device* pD3D12Device,
        _In_ REFIID riid,
        _COM_Outptr_ void** ppDSRDevice);

    END_INTERFACE
} ID3D12DSRDeviceFactoryVtbl;

interface ID3D12DSRDeviceFactory
{
    CONST_VTBL struct ID3D12DSRDeviceFactoryVtbl* lpVtbl;
};

EXTERN_C const GUID DECLSPEC_SELECTANY CLSID_D3D12DSRDeviceFactory;
EXTERN_C const GUID DECLSPEC_SELECTANY IID_ID3D12DSRDeviceFactory;
EXTERN_C const GUID DECLSPEC_SELECTANY IID_IDSRDevice;
EXTERN_C const GUID DECLSPEC_SELECTANY IID_IDSRSuperResEngine;
EXTERN_C const GUID DECLSPEC_SELECTANY IID_IDSRSuperResUpscaler;
EXTERN_C const GUID DECLSPEC_SELECTANY IID_IDirect3DDevice9On12;

typedef struct DSR_FLOAT2
{
    float X;
    float Y;
} DSR_FLOAT2;

typedef struct DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS
{
    ID3D12Resource* pTargetTexture;
    D3D12_RECT TargetRegion;
    ID3D12Resource* pSourceColorTexture;
    D3D12_RECT SourceColorRegion;
    ID3D12Resource* pSourceDepthTexture;
    D3D12_RECT SourceDepthRegion;
    ID3D12Resource* pMotionVectorsTexture;
    D3D12_RECT MotionVectorsRegion;
    DSR_FLOAT2 MotionVectorScale;
    DSR_FLOAT2 CameraJitter;
    float ExposureScale;
    float PreExposure;
    float Sharpness;
    float CameraNear;
    float CameraFar;
    float CameraFovAngleVert;
    ID3D12Resource* pExposureScaleTexture;
    ID3D12Resource* pIgnoreHistoryMaskTexture;
    D3D12_RECT IgnoreHistoryMaskRegion;
    ID3D12Resource* pReactiveMaskTexture;
    D3D12_RECT ReactiveMaskRegion;
} DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS;

typedef enum DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS
{
    DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_NONE = 0,
    DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_RESET_HISTORY = 0x1,
} DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS;

typedef interface IDSRSuperResUpscaler IDSRSuperResUpscaler;
typedef interface IDSRSuperResUpscalerVtbl
{
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE* QueryInterface)(
        IDSRSuperResUpscaler* This,
        REFIID riid,
        _COM_Outptr_  void** ppvObject);

    ULONG(STDMETHODCALLTYPE* AddRef)(
        IDSRSuperResUpscaler* This);

    ULONG(STDMETHODCALLTYPE* Release)(
        IDSRSuperResUpscaler* This);

    HRESULT(STDMETHODCALLTYPE* Execute)(
        IDSRSuperResUpscaler* This,
        _In_ const DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS* pParams,
        float TimeDeltaInSeconds,
        DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS Flags);

    END_INTERFACE
} IDSRSuperResUpscalerVtbl;

interface IDSRSuperResUpscaler
{
    CONST_VTBL struct IDSRSuperResUpscalerVtbl* lpVtbl;
};

typedef interface IDSRSuperResEngine IDSRSuperResEngine;
typedef interface IDSRSuperResEngineVtbl
{
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE* QueryInterface)(
        IDSRSuperResEngine* This,
        REFIID riid,
        _COM_Outptr_  void** ppvObject);

    ULONG(STDMETHODCALLTYPE* AddRef)(
        IDSRSuperResEngine* This);

    ULONG(STDMETHODCALLTYPE* Release)(
        IDSRSuperResEngine* This);

    HRESULT(STDMETHODCALLTYPE* CreateUpscaler)(
        IDSRSuperResEngine* This,
        _In_ ID3D12CommandQueue* pCommandQueue,
        _In_ REFIID iid,
        _COM_Outptr_ void** ppUpscaler);

    END_INTERFACE
} IDSRSuperResEngineVtbl;

interface IDSRSuperResEngine
{
    CONST_VTBL struct IDSRSuperResEngineVtbl* lpVtbl;
};

typedef interface IDirect3DDevice9On12 IDirect3DDevice9On12;
typedef interface IDirect3DDevice9On12Vtbl
{
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE* QueryInterface)(
        IDirect3DDevice9On12* This,
        REFIID riid,
        _COM_Outptr_  void** ppvObject);

    ULONG(STDMETHODCALLTYPE* AddRef)(
        IDirect3DDevice9On12* This);

    ULONG(STDMETHODCALLTYPE* Release)(
        IDirect3DDevice9On12* This);

    HRESULT(STDMETHODCALLTYPE* GetD3D12Device)(
        IDirect3DDevice9On12* This,
        REFIID riid,
        _COM_Outptr_  void** ppvDevice);

    HRESULT(STDMETHODCALLTYPE* GetD3D12Resource)(
        IDirect3DDevice9On12* This,
        _In_  IDirect3DResource9* pResource,
        REFIID riid,
        _COM_Outptr_  void** ppvResource);

    END_INTERFACE
} IDirect3DDevice9On12Vtbl;

interface IDirect3DDevice9On12
{
    CONST_VTBL struct IDirect3DDevice9On12Vtbl* lpVtbl;
};

#endif
