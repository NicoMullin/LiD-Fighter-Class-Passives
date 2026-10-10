/* With lid-fighter.dll (this plugin's old name) beside it, the plugin must
 * stand aside before it touches anything, so the two never make the same
 * changes twice - and say so in its log. */
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

    f = fopen("lid-fighter.dll", "wb");     /* the old name: a file is all it looks for */
    if (!f) { puts("could not make lid-fighter.dll"); return 2; }
    fclose(f);

    dll = LoadLibraryW(L"lid-classpassives.dll");
    if (!dll) { printf("LoadLibrary failed %lu\n", GetLastError()); return 2; }

    f = fopen("lid-classpassives.log", "rb");
    if (f) { fread(log, 1, sizeof log - 1, f); fclose(f); }
    printf("%s", log);
    if (!strstr(log, "stays out of its way") || !strstr(log, "nothing changed")) {
        puts("FAIL: the log does not say it stood aside for lid-fighter.dll");
        fails++;
    }
    if (strstr(log, "found ")) {
        puts("FAIL: it went looking for the game code anyway");
        fails++;
    }
    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails != 0;
}
