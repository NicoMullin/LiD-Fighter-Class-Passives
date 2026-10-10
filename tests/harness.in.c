/* Stand-in for BrgGame-Steam.exe: the status reset, a skill loop
 * with the five cases in the game's byte shapes, and the pause map's four
 * checks. Loads lid-classpassives.dll from its own folder and runs every case. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern const wchar_t str_th[], str_spy[], str_patrol[], str_lift[], str_item[], str_hpup[];
extern void fake_loop(void *status, void *array);
extern void fake_init(void *status);
extern int fake_render(void *ud, void *pm, void *mm, int *out);

int fake_cmp(const unsigned char *fstring, const wchar_t *lit)
{
    const wchar_t *s;
    memcpy(&s, fstring, sizeof s);
    return wcscmp(s, lit) == 0;
}

__asm__(
@ASM@);

static int g_eq[4];
static int eq0(void *u) { (void)u; return g_eq[0]; }
static int eq1(void *u) { (void)u; return g_eq[1]; }
static int eq2(void *u) { (void)u; return g_eq[2]; }
static int eq3(void *u) { (void)u; return g_eq[3]; }
static void *vtable[0x1060 / 8];
static struct { void **vt; } userdata = { vtable };
static unsigned char minimap[0x100];
static unsigned char pawn[0x4800];
static unsigned char uimanager[0x2E00];
static unsigned char pausemap[0x300];
static unsigned char elem[0x60];
static void *array[1] = { elem };
#define STATUS (pawn + 0x472C)
#define SPARE 0xFE000000u

static void set_ptr(unsigned char *at, const void *p) { memcpy(at, &p, sizeof p); }
static uint32_t flags(void) { uint32_t f; memcpy(&f, STATUS + 0x70, 4); return f; }
static void set_flags(uint32_t f) { memcpy(STATUS + 0x70, &f, 4); }

static int fails;
static void expect(const char *what, unsigned got, unsigned want)
{
    printf("%-50s got %08X want %08X  %s\n", what, got, want, got == want ? "ok" : "FAIL");
    if (got != want) fails++;
}
static void render(const char *what, int e0, int e1, int e2, int e3, int w0, int w1, int w2, int w3)
{
    int out[4] = {9, 9, 9, 9};
    g_eq[0] = e0; g_eq[1] = e1; g_eq[2] = e2; g_eq[3] = e3;
    fake_render(&userdata, pausemap, minimap, out);
    expect(what, (unsigned)(out[0] << 12 | out[1] << 8 | out[2] << 4 | out[3]),
           (unsigned)(w0 << 12 | w1 << 8 | w2 << 4 | w3));
}
static void skill(const wchar_t *type)       /* one fighter with one skill */
{
    fake_init(STATUS);
    set_ptr(elem + 0x34, type);
    fake_loop(STATUS, array);
}

int main(void)
{
    HMODULE dll;

    vtable[0x1038 / 8] = (void *)eq0; vtable[0x1040 / 8] = (void *)eq1;
    vtable[0x1048 / 8] = (void *)eq2; vtable[0x1050 / 8] = (void *)eq3;
    set_ptr(pausemap + 0x68, uimanager);
    set_ptr(uimanager + 0x2D4C, pawn);

    puts("-- before the DLL (map results: TH/Spy, enemies, lifts, items)");
    set_flags(0xFFFFFFFF); fake_init(STATUS);
    expect("reset keeps the spare bits (stock)", flags() & SPARE, SPARE);
    set_flags(0); skill(str_patrol);
    expect("PATROL DUTY sets nothing (stock)", flags() & ~0x209Fu, 0);
    render("map, nothing equipped (stock)", 0,0,0,0, 0,0,0,0);

    dll = LoadLibraryW(L"lid-classpassives.dll");
    if (!dll) { printf("LoadLibrary failed %lu\n", GetLastError()); return 2; }
    puts("-- with the DLL");

    set_flags(0xFFFFFFFF); fake_init(STATUS);
    expect("reset clears the spare bits", flags() & SPARE, 0);
    expect("reset leaves the game's own bits as before", flags(), 0x209F);

    skill(str_th);     expect("Treasure Hunter -> bSearchUpItem", flags() & ~0x209Fu, 0x00100000);
    render("  map: Treasure Hunter passive", 0,0,0,0, 1,0,0,0);
    skill(str_spy);    expect("Spy -> bit 25", flags() & ~0x209Fu, 0x02000000);
    render("  map: Spy passive", 0,0,0,0, 1,0,0,0);
    skill(str_patrol); expect("PATROL DUTY -> bit 26", flags() & ~0x209Fu, 0x04000000);
    render("  map: PATROL DUTY passive", 0,0,0,0, 0,1,0,0);
    skill(str_lift);   expect("Iwazaru -> bit 27", flags() & ~0x209Fu, 0x08000000);
    render("  map: Iwazaru passive", 0,0,0,0, 0,0,1,0);
    skill(str_item);   expect("Coyote Smith -> bit 28", flags() & ~0x209Fu, 0x10000000);
    render("  map: Coyote Smith passive", 0,0,0,0, 0,0,0,1);
    skill(str_hpup);   expect("switch to an unrelated skill: bits cleared", flags() & ~0x209Fu, 0);
    render("  map: fighter without them", 0,0,0,0, 0,0,0,0);
    render("  map: every decal really equipped", 1,1,1,1, 1,1,1,1);
    render("  map: only PATROL DUTY equipped", 0,1,0,0, 0,1,0,0);

    set_flags(0xFFFFFFFFu & ~SPARE & ~0x00100000u);
    render("map: every game flag except Treasure Hunter", 0,0,0,0, 0,0,0,0);
    set_ptr(uimanager + 0x2D4C, NULL);
    render("map: no fighter (menus/loading)", 0,0,0,0, 0,0,0,0);
    render("map: no fighter, decals equipped", 1,0,1,0, 1,0,1,0);
    set_ptr(pausemap + 0x68, NULL);
    render("map: no UI manager", 0,0,0,0, 0,0,0,0);

    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails != 0;
}
