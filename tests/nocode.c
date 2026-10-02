/* Stand-in for a BrgGame-Steam.exe the DLL does not recognise, as after a game
 * update that moves the code. The DLL must change nothing, say so in its log,
 * and still pass DirectInput through. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    typedef HRESULT (WINAPI *create_fn)(HINSTANCE, DWORD, const GUID *, void **, void *);
    static const GUID IID_IDirectInput8W =
        {0xBF798031, 0x483A, 0x4DA2, {0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00}};
    char log[4096] = {0};
    void *di = NULL;
    create_fn create;
    HMODULE dll;
    HRESULT hr;
    FILE *f;
    int fails = 0;

    dll = LoadLibraryW(L"dinput8.dll");
    if (!dll) { printf("LoadLibrary failed %lu\n", GetLastError()); return 2; }

    f = fopen("lid_dll.log", "rb");
    if (f) { fread(log, 1, sizeof log - 1, f); fclose(f); }
    printf("%s", log);
    if (!strstr(log, "found 0 times") || !strstr(log, "nothing changed")) {
        puts("FAIL: the log does not say the code was not found and nothing changed");
        fails++;
    }
    if (strstr(log, "now ")) {
        puts("FAIL: the log says something was changed");
        fails++;
    }

    create = (create_fn)GetProcAddress(dll, "DirectInput8Create");
    hr = create ? create(GetModuleHandleW(NULL), 0x0800, &IID_IDirectInput8W, &di, NULL) : E_FAIL;
    printf("DirectInput8Create passes through: %s\n", hr == S_OK ? "ok" : "FAIL");
    if (hr != S_OK) fails++;
    if (di) {
        typedef ULONG (WINAPI *release_fn)(void *);
        ((release_fn)(*(void ***)di)[2])(di);
    }
    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails != 0;
}
