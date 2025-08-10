#include <windows.h>
#include <stdio.h>
#include "ddraw.h"

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main()
{
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "DSRTest";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("DSRTest", "DSR Test", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, GetModuleHandle(NULL), NULL);
    ShowWindow(hwnd, SW_SHOW);

    HRESULT(WINAPI * DirectDrawCreate)(GUID FAR*, LPDIRECTDRAW FAR*, IUnknown FAR*);
    DirectDrawCreate = (void*)GetProcAddress(LoadLibraryA("ddraw.dll"), "DirectDrawCreate");

    if (!DirectDrawCreate)
    {
        printf("DirectDrawCreate not found\n");
        return 1;
    }

    LPDIRECTDRAW dd = NULL;
    if (FAILED(DirectDrawCreate(NULL, &dd, NULL)))
    {
        printf("DirectDrawCreate failed\n");
        return 1;
    }

    if (FAILED(IDirectDraw_SetCooperativeLevel(dd, hwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN)))
    {
        printf("SetCooperativeLevel failed\n");
        return 1;
    }

    if (FAILED(IDirectDraw_SetDisplayMode(dd, 640, 480, 32)))
    {
        printf("SetDisplayMode failed\n");
        return 1;
    }

    DDSURFACEDESC ddsd = { 0 };
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    ddsd.dwBackBufferCount = 1;

    LPDIRECTDRAWSURFACE primary_surface = NULL;
    if (FAILED(IDirectDraw_CreateSurface(dd, &ddsd, &primary_surface, NULL)))
    {
        printf("CreateSurface failed\n");
        return 1;
    }

    LPDIRECTDRAWSURFACE back_buffer = NULL;
    DDSCAPS caps = { DDSCAPS_BACKBUFFER };
    if (FAILED(IDirectDrawSurface_GetAttachedSurface(primary_surface, &caps, &back_buffer)))
    {
        printf("GetAttachedSurface failed\n");
        return 1;
    }

    MSG msg = { 0 };
    int frame_count = 0;
    while (msg.message != WM_QUIT && frame_count < 10)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            IDirectDrawSurface_Flip(primary_surface, NULL, DDFLIP_WAIT);
            frame_count++;
        }
    }

    IDirectDrawSurface_Release(back_buffer);
    IDirectDrawSurface_Release(primary_surface);
    IDirectDraw_Release(dd);

    return 0;
}
