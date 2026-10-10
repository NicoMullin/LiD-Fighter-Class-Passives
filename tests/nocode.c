/* Stand-in for a BrgGame-Steam.exe the plugin does not recognise, as after a
 * game update that moves the code. The plugin must change nothing and say so in
 * its log. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char log[4096] = {0};
    HMODULE dll;
    FILE *f;
    int fails = 0;

    dll = LoadLibraryW(L"lid-classpassives.dll");
    if (!dll) { printf("LoadLibrary failed %lu\n", GetLastError()); return 2; }

    f = fopen("lid-classpassives.log", "rb");
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
    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails != 0;
}
