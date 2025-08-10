#include <windows.h>
#include <stdio.h>
#include "ddraw.h"

int main()
{
    HRESULT(WINAPI * DirectDrawCreate)(GUID FAR*, LPDIRECTDRAW FAR*, IUnknown FAR*);
    DirectDrawCreate = (void*)GetProcAddress(LoadLibraryA("ddraw.dll"), "DirectDrawCreate");

    if (DirectDrawCreate)
    {
        printf("DirectDrawCreate found\n");
        DirectDrawCreate(NULL, NULL, NULL);
        printf("DirectDrawCreate called\n");
    }
    else
    {
        printf("DirectDrawCreate not found\n");
    }

    return 0;
}
