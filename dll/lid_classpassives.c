/*
 * lid-classpassives.dll for Let It Die - a plugin for the LiD Plugin Loader that
 * changes game code in memory only.
 *
 * The loader (dinput8.dll) loads this from Binaries\Win64\lid-plugins when the
 * game starts. It changes the running game (never the exe on disk) so the
 * pause map's reveals follow the fighter's skill list, not only the decals
 * equipped in the save. That is where a fighter-type passive from BrgGame.upk
 * lands.
 *
 *   Decal          Pause map shows                  Fighter flag
 *   Treasure Hunter everything hidden               bSearchUpItem (the game's own)
 *   Spy            everything hidden                spare bit 25
 *   PATROL DUTY    enemies                          spare bit 26
 *   Iwazaru        escalators and elevators         spare bit 27
 *   Coyote Smith   items                            spare bit 28
 *
 * The game's skill code already sets bSearchUpItem for Treasure Hunter. For the
 * other four it only changes stamina or reward rates, so their compare call in
 * that code is sent through a small stub that also sets a spare bit in the same
 * flag word (bits 25-31 are not used by the game). The status reset is changed
 * to clear those spare bits too, so they never outlive the skill list.
 *
 * Since 2.0.5, Treasure Hunter or Spy (decal or class passive) also lifts the
 * map's fog: each floor's whole map is uncovered as you arrive, on the pause
 * map and the HUD minimap. The game uncovers the fog by queuing "sprays" as you
 * walk (UIMiniMapManager.MiniMap_AddArrivedPoint(hexagon, x, y), x and y 0-128
 * across a map hexagon) and marking the hexagon's explored grid; its paint call
 * (MiniMap_UpdateArrivedRate, every frame) draws the sprays. The paint call's
 * slot in the minimap's function table is sent through a small function that
 * first sprays every hexagon (5 x 5, at 1.5 x the walking size) and marks its
 * grid, once per floor and area.
 *
 * Safety rules:
 *   - Every piece is found by its exact bytes and must be found exactly once.
 *     Anything not found is left alone and named in the log. After a game
 *     update that moves the code, the DLL does nothing at all.
 *   - Changed in the game: one byte in the status reset, four 5-byte calls in
 *     the skill code, four calls in the pause map and one pointer in the
 *     minimap's function table.
 *   - No network, no registry. The only file it writes is lid-classpassives.log next
 *     to itself.
 *   - Delete lid-classpassives.dll and the game is exactly stock again.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "version.h"

/* ------------------------------------------------------------------ log */

static wchar_t g_log_path[MAX_PATH];
static int g_log_started;

static void log_line(const char *fmt, ...)
{
    char msg[512];
    SYSTEMTIME t;
    va_list ap;
    int n;
    HANDLE f;
    DWORD written;

    if (!g_log_path[0])
        return;
    GetLocalTime(&t);
    n = snprintf(msg, sizeof msg, "%02u:%02u:%02u  ", t.wHour, t.wMinute, t.wSecond);
    va_start(ap, fmt);
    n += vsnprintf(msg + n, sizeof msg - n - 2, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof msg - 3)
        n = (int)sizeof msg - 3;
    msg[n++] = '\r';
    msg[n++] = '\n';

    /* a fresh log each time the game starts */
    f = CreateFileW(g_log_path, g_log_started ? FILE_APPEND_DATA : GENERIC_WRITE,
                    FILE_SHARE_READ, NULL, g_log_started ? OPEN_ALWAYS : CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    g_log_started = 1;
    WriteFile(f, msg, (DWORD)n, &written, NULL);
    CloseHandle(f);
}

/* ------------------------------------------------ what gets changed */

/* Field offsets, from the CodeRed SDK dump of 5.0.4.2 and checked against the
 * game's own minimap code, which reads the same chain. */
#define PAUSEMAP_UIMANAGER   0x68     /* UBrgUIMenuPart_PauseMap -> UBrgUIManager */
#define UIMANAGER_PLAYERPAWN 0x2D4C   /* mPlayerCommonPawnNative                  */

/* Where the fighter's skill flag word (mSkillStatus + 0x70) sits in the pawn.
 * It moves between game builds (0x479C up to 1.92, 0x47BC in 1.93), so it is
 * read from the game's own HUD minimap code at start-up (see find_flag_word);
 * 0 = not found, and then only really equipped decals count. */
static uint32_t g_flags_at;

#define FLAG_SEARCHUP_ITEM 0x00100000u /* bSearchUpItem, set by Treasure Hunter */
#define BIT_SPY            0x02000000u
#define BIT_PATROL         0x04000000u
#define BIT_LIFT           0x08000000u
#define BIT_ITEM           0x10000000u
#define SPARE_BITS         0xFE000000u

/* decals whose skill-code case gets a spare bit */
struct mark {
    const wchar_t *type;
    uint32_t bit;
    const char *decal;
    unsigned char *call;        /* the compare call in the skill code */
};
static struct mark g_marks[] = {
    {L"SKLTP_ADVENTURE", BIT_SPY, "Spy", NULL},
    {L"SKLTP_PATROL_WOT", BIT_PATROL, "PATROL DUTY", NULL},
    {L"SKLTP_SEARCHLIFT_SPIRITUP_K7", BIT_LIFT, "Iwazaru", NULL},
    {L"SKLTP_SEARCHITEM_MONEYUP_K7", BIT_ITEM, "Coyote Smith", NULL},
};
#define MARK_COUNT (sizeof g_marks / sizeof g_marks[0])

/* the pause map's four save checks (UserData vtable slots) */
struct check {
    uint32_t slot;
    uint32_t flags;             /* fighter flag bits that also count as "yes" */
    const char *what;
    unsigned char *call;
    int last;
};
static struct check g_checks[] = {
    {0x1038, FLAG_SEARCHUP_ITEM | BIT_SPY, "everything (Treasure Hunter / Spy)", NULL, -1},
    {0x1040, BIT_PATROL, "enemies (PATROL DUTY)", NULL, -1},
    {0x1048, BIT_LIFT, "escalators and elevators (Iwazaru)", NULL, -1},
    {0x1050, BIT_ITEM, "items (Coyote Smith)", NULL, -1},
};
#define CHECK_COUNT (sizeof g_checks / sizeof g_checks[0])

static const unsigned char *read_ptr(const unsigned char *at)
{
    const unsigned char *p;
    memcpy(&p, at, sizeof p);
    return p;
}

/* Called by the pause-map stubs with the pause map, what the save check said
 * and which check it was. Returns what the game should see. */
int map_check(const unsigned char *pausemap, int equipped, int which)
{
    struct check *c = &g_checks[which];
    const unsigned char *ui, *pawn;
    uint32_t flags = 0;
    int passive = 0, result, state;

    ui = pausemap ? read_ptr(pausemap + PAUSEMAP_UIMANAGER) : NULL;
    pawn = ui ? read_ptr(ui + UIMANAGER_PLAYERPAWN) : NULL;
    if (pawn && g_flags_at) {
        memcpy(&flags, pawn + g_flags_at, sizeof flags);
        passive = (flags & c->flags) != 0;
    }
    result = (equipped == 1 || passive) ? 1 : equipped;

    /* the map is drawn every frame; log only when the answer changes */
    state = (equipped == 1) | (passive << 1) | ((pawn != NULL) << 2);
    if (state != c->last) {
        c->last = state;
        log_line("pause map, %s: equipped decal=%s, fighter passive=%s -> %s",
                 c->what, equipped == 1 ? "yes" : "no",
                 pawn ? (passive ? "yes" : "no") : "no fighter",
                 result == 1 ? "SHOWN" : "hidden");
    }
    return result;
}

/* ------------------------------------------------------------ helpers */

struct span {
    unsigned char *start;
    size_t size;
};

static int section(const char *name, struct span *out)
{
    unsigned char *base = (unsigned char *)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt;
    IMAGE_SECTION_HEADER *sec;
    WORD i;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (strncmp((const char *)sec->Name, name, IMAGE_SIZEOF_SHORT_NAME) == 0) {
            out->start = base + sec->VirtualAddress;
            out->size = sec->Misc.VirtualSize;
            return 1;
        }
    }
    return 0;
}

/* pattern with a mask: 'x' = must match, '?' = any byte */
static int matches(const unsigned char *p, const unsigned char *pat, const char *mask)
{
    size_t i;
    for (i = 0; mask[i]; i++)
        if (mask[i] == 'x' && p[i] != pat[i])
            return 0;
    return 1;
}

static int find_all(struct span s, const unsigned char *pat, const char *mask, unsigned char **last)
{
    size_t i, n = strlen(mask);
    int hits = 0;
    for (i = 0; i + n <= s.size; i++) {
        if (s.start[i] == pat[0] && matches(s.start + i, pat, mask)) {
            hits++;
            *last = s.start + i;
        }
    }
    return hits;
}

static int32_t read32(const unsigned char *p)
{
    int32_t v;
    memcpy(&v, p, 4);
    return v;
}

static int rel32(const unsigned char *next, const unsigned char *target, int32_t *out)
{
    intptr_t d = target - next;
    if (d != (int32_t)d)
        return 0;
    *out = (int32_t)d;
    return 1;
}

static int write_code(unsigned char *at, const void *bytes, size_t n)
{
    DWORD old;
    if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
    return 1;
}

/* One page within 2 GB of the game code, for the stubs. */
static unsigned char *g_page;
static size_t g_page_used;

static unsigned char *page_near(unsigned char *code)
{
    const ULONG_PTR step = 0x10000;
    ULONG_PTR origin = (ULONG_PTR)code & ~(step - 1);
    ULONG_PTR d;

    for (d = step; d < 0x70000000; d += step) {
        void *p = VirtualAlloc((void *)(origin - d), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!p)
            p = VirtualAlloc((void *)(origin + d), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (p)
            return p;
    }
    return NULL;
}

static unsigned char *emit(const unsigned char *code, size_t n)
{
    unsigned char *at;
    if (g_page_used + n > 4096)
        return NULL;
    at = g_page + g_page_used;
    memcpy(at, code, n);
    g_page_used = (g_page_used + n + 15) & ~(size_t)15;
    return at;
}

/* ------------------------------------------------- finding the pieces */

/* Pause map: test dword [rax+0xCC],0x200 / jne ?? / mov rax,[rcx] /
 * mov [rsp+0x60],r13 / call [rax+0x1038]. Found once in 1.89 and 2026-09. */
static const unsigned char MAP_PATTERN[] = {
    0xF7, 0x80, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x0F, 0x85, 0x00, 0x00, 0x00, 0x00,
    0x48, 0x8B, 0x01,
    0x4C, 0x89, 0x6C, 0x24, 0x60,
    0xFF, 0x90, 0x38, 0x10, 0x00, 0x00,
};
static const char MAP_MASK[] = "xxxxxxxxxxxx????xxxxxxxxxxxxxx";
#define MAP_CALL_AT 24
#define MAP_WINDOW  0x100   /* the other three checks follow within this */

/* Status reset: mov eax,[rbx+0x70] / and eax,0xFE00209F / bts eax,7 /
 * mov [rbx+0x70],eax. The top byte of the mask keeps bits 25-31. */
static const unsigned char RESET_PATTERN[] = {
    0x8B, 0x43, 0x70, 0x25, 0x9F, 0x20, 0x00, 0xFE, 0x0F, 0xBA, 0xE8, 0x07, 0x89, 0x43, 0x70,
};
static const char RESET_MASK[] = "xxxxxxxxxxxxxxx";
#define RESET_MASK_TOP 7

/* Skill code, one case per effect type:
 * lea rdx,[rip+L"SKLTP_..."] / add rcx,0x34 / add rcx,rdi / call compare */
static const unsigned char CASE_PATTERN[] = {
    0x48, 0x8D, 0x15, 0, 0, 0, 0, 0x48, 0x83, 0xC1, 0x34, 0x48, 0x03, 0xCF, 0xE8,
};
static const char CASE_MASK[] = "xxx????xxxxxxxx";
#define CASE_CALL_AT 14
/* after Treasure Hunter's compare: test eax,eax / je +12 / or dword [rsi+0x70],0x100000
 * - proves rsi holds the skill status in this build */
static const unsigned char ANCHOR_AFTER[] = {0x85, 0xC0, 0x74, 0x0C, 0x81, 0x4E, 0x70, 0x00, 0x00, 0x10, 0x00};
#define CASE_WINDOW 0x10000

#define MAX_COPIES 8

/* where the UTF-16 string `s` (with its terminator, starting a string) sits */
static int find_wstr(struct span rdata, const wchar_t *s, unsigned char **out)
{
    size_t len = (wcslen(s) + 1) * sizeof(wchar_t), i;
    int n = 0;
    for (i = 2; i + len <= rdata.size; i += 2) {
        unsigned char *p = rdata.start + i;
        if (p[0] == (unsigned char)s[0] && p[-1] == 0 && p[-2] == 0 && memcmp(p, s, len) == 0) {
            if (n < MAX_COPIES)
                out[n] = p;
            n++;
        }
    }
    return n;
}

/* the one compare call in the skill code for `type`, or NULL */
static unsigned char *find_case(struct span text, struct span rdata, const wchar_t *type)
{
    unsigned char *strs[MAX_COPIES], *hit = NULL;
    int nstr = find_wstr(rdata, type, strs), hits = 0, k;
    size_t i, n = sizeof CASE_PATTERN;

    if (nstr < 1 || nstr > MAX_COPIES)
        return NULL;
    for (i = 0; i + n + 4 <= text.size; i++) {
        unsigned char *p = text.start + i, *target;
        if (p[0] != 0x48 || !matches(p, CASE_PATTERN, CASE_MASK))
            continue;
        target = p + 7 + read32(p + 3);
        for (k = 0; k < nstr; k++) {
            if (target == strs[k]) {
                hits++;
                hit = p + CASE_CALL_AT;
            }
        }
    }
    return hits == 1 ? hit : NULL;
}

/* HUD minimap: mov rax,[rcx+0x2D4C] / test dword [rax+X],0x100000 - X is the
 * fighter's skill flag word (bSearchUpItem = Treasure Hunter). */
static const unsigned char FLAGS_PATTERN[] = {
    0x48, 0x8B, 0x81, 0x4C, 0x2D, 0x00, 0x00, 0xF7, 0x80, 0, 0, 0, 0, 0x00, 0x00, 0x10, 0x00,
};
static const char FLAGS_MASK[] = "xxxxxxxxx????xxxx";
#define FLAGS_DISP_AT 9

static uint32_t find_flag_word(struct span text)
{
    unsigned char *hit = NULL;
    uint32_t at;
    int n = find_all(text, FLAGS_PATTERN, FLAGS_MASK, &hit);

    if (n != 1) {
        log_line("fighter flags: HUD minimap code found %d times (needs exactly 1)", n);
        return 0;
    }
    memcpy(&at, hit + FLAGS_DISP_AT, sizeof at);
    if (at < 0x1000 || at > 0x10000 || (at & 3)) {
        log_line("fighter flags: the HUD minimap code reads offset 0x%X, which does not look right", at);
        return 0;
    }
    return at;
}

static unsigned char *call_target(unsigned char *call)
{
    return call + 5 + read32(call + 1);
}

/* ------------------------------------------------------- the map's fog */

/* UIMiniMapManager.MiniMap_UpdateArrivedRate(HUD), the start: mov rax,rsp /
 * mov [rax+0x10],rdx / push rdi / push r12 / sub rsp,0x118 /
 * cmp qword [rcx+0xBC],0 (mUnitArrivedRateTexture) / mov r12,rdx / mov rdi,rcx / je */
static const unsigned char UPDATE_PATTERN[] = {
    0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x57, 0x41, 0x54, 0x48, 0x81, 0xEC, 0x18, 0x01, 0x00,
    0x00, 0x48, 0x83, 0xB9, 0xBC, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x8B, 0xE2, 0x48, 0x8B, 0xF9, 0x0F,
    0x84,
};
static const char UPDATE_MASK[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
#define UPDATE_SLOT 0x450
#define ADD_SLOT    0x350

/* MiniMap_AddArrivedPoint(cell, x, y), the start: mov [rsp+8],rbx / push rdi /
 * sub rsp,0x60 / mov eax,[rcx+0x94] (mSprayLifeTime) / movsxd rdi,[rcx+0x11C]
 * (mSprayObjects' count) / mov rbx,rcx */
static const unsigned char ADD_START[] = {
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x60, 0x8B, 0x81, 0x94, 0x00, 0x00, 0x00,
    0x48, 0x63, 0xB9, 0x1C, 0x01, 0x00, 0x00, 0x48, 0x8B, 0xD9,
};

/* Pieces of those two functions that show the layout below is this build's. */
struct piece {
    const unsigned char *bytes;
    size_t size;
    const char *what;
};
static const struct piece UPDATE_PIECES[] = {
    {(const unsigned char *)"\x8B\x87\xCC\x00\x00\x00\xA8\x01", 8, "the new-floor flag (+0xCC)"},
    {(const unsigned char *)"\x48\x8B\x87\x40\x01\x00\x00\x42\x3B\x6C\x38\x24", 12, "the hexagons (+0x140, grid count +0x24)"},
    {(const unsigned char *)"\x4B\x8B\x44\x3E\x1C", 5, "a hexagon's grid (+0x1C)"},
    {(const unsigned char *)"\x4D\x6B\xFF\x2C", 4, "a hexagon's size (0x2C)"},
};
static const struct piece ADD_PIECES[] = {
    {(const unsigned char *)"\x48\x03\x83\x14\x01\x00\x00", 7, "the sprays (+0x114)"},
    {(const unsigned char *)"\x48\x6B\xC0\x1C", 4, "a spray's size (0x1C)"},
    {(const unsigned char *)"\xF3\x0F\x11\x48\x04", 5, "a spray's x (+4)"},
    {(const unsigned char *)"\xF3\x0F\x11\x78\x18", 5, "a spray's scale (+0x18)"},
};

#define MM_UIMANAGER  0x60     /* UIMiniMapManager -> UIManager                   */
#define MM_FLAGS      0xCC     /* bit 0 mbUnitArrivedRateNewFlag, bit 1 AlwaysZero */
#define MM_SPRAYS     0x114    /* TArray<SprayObject>: data, count at +8          */
#define MM_CELLS      0x140    /* TArray<MiniMapCellInfo>: data, count at +8      */
#define CELL_SIZE     0x2C
#define CELL_FAR      0x00     /* bit 0 mIsFarCell                                */
#define CELL_RATES    0x1C     /* TArray<float> mGridArrivedRates (128 x 128)     */
#define SPRAY_SIZE    0x1C
#define SPRAY_X       0x04
#define SPRAY_SCALE   0x18
#define FOG_GRID      5        /* sprays per hexagon side: 0, 32, 64, 96, 128     */
#define FOG_SCALE     1.5f     /* against walking's 1 (lid-speedrun 0.0.21, proven) */

typedef void (*update_fn)(void *minimap, void *hud);
typedef void (*add_fn)(void *minimap, int cell, int x, int y);
static update_fn g_update_orig;
static add_fn g_add_point;

static int has_piece(const unsigned char *fn, size_t span, const struct piece *p)
{
    size_t i;
    for (i = 0; i + p->size <= span; i++)
        if (memcmp(fn + i, p->bytes, p->size) == 0)
            return 1;
    return 0;
}

/* Treasure Hunter or Spy on the fighter now (decal or class passive). */
static int fog_wanted(const unsigned char *mm)
{
    const unsigned char *ui = read_ptr(mm + MM_UIMANAGER), *pawn;
    uint32_t flags;

    if (!ui || !g_flags_at || !(pawn = read_ptr(ui + UIMANAGER_PLAYERPAWN)))
        return 0;
    memcpy(&flags, pawn + g_flags_at, sizeof flags);
    return (flags & (FLAG_SEARCHUP_ITEM | BIT_SPY)) != 0;
}

static void uncover(unsigned char *mm)
{
    unsigned char *cells = (unsigned char *)read_ptr(mm + MM_CELLS);
    int32_t n = read32(mm + MM_CELLS + 8), i, k, x, y, done = 0, sprays = 0;
    uint32_t flags;

    memcpy(&flags, mm + MM_FLAGS, sizeof flags);
    if (flags & 3)
        return;             /* the next spray wipes the map for this floor: wait */
    if (!cells || n <= 0 || n > 4096)
        return;
    /* A hexagon's grid corner (0, 0) lies outside the hexagon, so walking never
     * marks it: 1 there means it was uncovered here, on this floor. */
    for (i = 0; i < n; i++) {
        unsigned char *c = cells + (size_t)i * CELL_SIZE;
        float *rates = (float *)read_ptr(c + CELL_RATES);
        int32_t rn = read32(c + CELL_RATES + 8);
        if (!(c[CELL_FAR] & 1) && rates && rn > 0 && rn <= 0x10000 && rates[0] != 1.0f)
            break;
    }
    if (i == n)
        return;             /* all uncovered already */

    for (i = 0; i < n; i++) {
        unsigned char *c = cells + (size_t)i * CELL_SIZE;
        float *rates = (float *)read_ptr(c + CELL_RATES);
        int32_t rn = read32(c + CELL_RATES + 8);

        if ((c[CELL_FAR] & 1) || !rates || rn <= 0 || rn > 0x10000)
            continue;       /* scenery seen from afar, or no grid */
        for (k = 0; k < rn; k++)
            rates[k] = 1.0f;
        done++;
        for (y = 0; y < FOG_GRID; y++)
            for (x = 0; x < FOG_GRID; x++) {
                int sx = x * 128 / (FOG_GRID - 1), sy = y * 128 / (FOG_GRID - 1);
                unsigned char *sd;
                int32_t sn;

                g_add_point(mm, i, sx, sy);
                sd = (unsigned char *)read_ptr(mm + MM_SPRAYS);
                sn = read32(mm + MM_SPRAYS + 8);
                if (sd && sn > 0 && read32(sd + (size_t)(sn - 1) * SPRAY_SIZE + SPRAY_X) == sx) {
                    float scale = FOG_SCALE;
                    memcpy(sd + (size_t)(sn - 1) * SPRAY_SIZE + SPRAY_SCALE, &scale, sizeof scale);
                    sprays++;
                }
            }
    }
    log_line("map fog: uncovered %d of %d hexagons (%d sprays)", done, n, sprays);
}

static void update_hook(void *minimap, void *hud)
{
    if (minimap && fog_wanted(minimap))
        uncover(minimap);
    g_update_orig(minimap, hud);
}

/* Find the paint call and its slot in the minimap's function table, check the
 * layout, and send the slot through update_hook. */
static void hook_fog(struct span text, struct span rdata)
{
    unsigned char *update = NULL, **slot = NULL, *add;
    size_t i;
    int n, slots = 0;
    DWORD old;

    if (!g_flags_at) {
        log_line("map fog: left alone (the fighter flags were not found)");
        return;
    }
    n = find_all(text, UPDATE_PATTERN, UPDATE_MASK, &update);
    if (n != 1) {
        log_line("map fog: the minimap's paint code found %d times (needs exactly 1) - left alone", n);
        return;
    }
    for (i = 0; i + 8 <= rdata.size; i += 8) {
        if (read_ptr(rdata.start + i) == update) {
            slots++;
            slot = (unsigned char **)(rdata.start + i);
        }
    }
    if (slots != 1 || (unsigned char *)slot < rdata.start + UPDATE_SLOT) {
        log_line("map fog: the paint code is in %d function tables (needs exactly 1) - left alone", slots);
        return;
    }
    add = *(unsigned char **)((unsigned char *)slot - UPDATE_SLOT + ADD_SLOT);
    if (add < text.start || add >= text.start + text.size - 0x100 ||
        memcmp(add, ADD_START, sizeof ADD_START) != 0) {
        log_line("map fog: the spray code is not where it should be - left alone");
        return;
    }
    for (i = 0; i < sizeof UPDATE_PIECES / sizeof UPDATE_PIECES[0]; i++)
        if (!has_piece(update, 0x400, &UPDATE_PIECES[i])) {
            log_line("map fog: the paint code does not use %s as expected - left alone", UPDATE_PIECES[i].what);
            return;
        }
    for (i = 0; i < sizeof ADD_PIECES / sizeof ADD_PIECES[0]; i++)
        if (!has_piece(add, 0x100, &ADD_PIECES[i])) {
            log_line("map fog: the spray code does not use %s as expected - left alone", ADD_PIECES[i].what);
            return;
        }
    g_update_orig = (update_fn)(void *)update;
    g_add_point = (add_fn)(void *)add;
    if (!VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old)) {
        log_line("map fog: could not change the minimap's function table - left alone");
        return;
    }
    *slot = (unsigned char *)(void *)update_hook;
    VirtualProtect(slot, sizeof *slot, old, &old);
    log_line("map fog: Treasure Hunter / Spy now uncover each floor's whole map (game+0x%llX)",
             (unsigned long long)((unsigned char *)slot - (unsigned char *)GetModuleHandleW(NULL)));
}

/* ------------------------------------------------------ the changes */

static void patch_game(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleW(NULL);
    struct span text, rdata;
    unsigned char *hit = NULL, *reset = NULL, *anchor, *compare = NULL;
    int n, spare_ok = 0, checks_done = 0, marks_done = 0;
    size_t i, j;

    if (!section(".text", &text) || !section(".rdata", &rdata)) {
        log_line("could not read the game's code sections - nothing changed");
        return;
    }

    /* 1. the pause map: required, or nothing else is worth doing */
    n = find_all(text, MAP_PATTERN, MAP_MASK, &hit);
    if (n != 1) {
        log_line("pause map code found %d times (needs exactly 1) - nothing changed. "
                 "A game update may have moved it.", n);
        return;
    }
    g_checks[0].call = hit + MAP_CALL_AT;
    g_flags_at = find_flag_word(text);
    if (g_flags_at)
        log_line("fighter flags: at fighter+0x%X (read from the HUD minimap code)", g_flags_at);
    else
        log_line("fighter flags: not found - the map reveals follow equipped decals only, "
                 "class passives do not count");
    hook_fog(text, rdata);
    for (i = 1; i < CHECK_COUNT; i++) {
        unsigned char want[6] = {0xFF, 0x90, 0, 0x10, 0, 0};
        unsigned char *from = g_checks[0].call + 6, *found = NULL;
        int count = 0;
        want[2] = (unsigned char)(g_checks[i].slot & 0xFF);
        for (j = 0; j < MAP_WINDOW; j++) {
            if (memcmp(from + j, want, sizeof want) == 0) {
                count++;
                found = from + j;
            }
        }
        if (count == 1)
            g_checks[i].call = found;
        else
            log_line("pause map, %s: check found %d times - left alone", g_checks[i].what, count);
    }

    /* 2. the skill code: Treasure Hunter's case proves the layout, the reset
     *    must be clearable, then each decal's own case */
    anchor = find_case(text, rdata, L"SKLTP_SEARCHUP_ITEM");
    n = find_all(text, RESET_PATTERN, RESET_MASK, &reset);
    if (!anchor || memcmp(anchor + 5, ANCHOR_AFTER, sizeof ANCHOR_AFTER) != 0)
        log_line("skill code: Treasure Hunter's case is not as expected - only Treasure Hunter will work");
    else if (n != 1)
        log_line("skill code: status reset found %d times (needs exactly 1) - only Treasure Hunter will work", n);
    else {
        spare_ok = 1;
        compare = call_target(anchor);
    }
    for (i = 0; spare_ok && i < MARK_COUNT; i++) {
        unsigned char *c = find_case(text, rdata, g_marks[i].type);
        intptr_t d = c ? c - anchor : 0;
        if (!c || d > CASE_WINDOW || d < -CASE_WINDOW || call_target(c) != compare)
            log_line("skill code: %s's case not found - left alone", g_marks[i].decal);
        else
            g_marks[i].call = c;
    }
    if (!spare_ok) {
        g_checks[0].flags = FLAG_SEARCHUP_ITEM;
        for (i = 1; i < CHECK_COUNT; i++)
            g_checks[i].call = NULL;
    }

    /* 3. build the stubs */
    g_page = page_near(g_checks[0].call);
    if (!g_page) {
        log_line("no memory free near the game code - nothing changed");
        return;
    }
    {
        unsigned char *map_stub[CHECK_COUNT] = {0}, *mark_stub[MARK_COUNT] = {0};
        unsigned char code[64];
        void *fn = (void *)map_check;
        DWORD old;
        int32_t r;

        for (i = 0; i < CHECK_COUNT; i++) {
            uint32_t which = (uint32_t)i;
            if (!g_checks[i].call)
                continue;
            /* sub rsp,28 / call [rax+slot] / mov edx,eax / mov rcx,rdi /
             * mov r8d,which / mov rax,map_check / call rax / add rsp,28 / ret */
            memcpy(code, "\x48\x83\xEC\x28\xFF\x90", 6);
            memcpy(code + 6, &g_checks[i].slot, 4);
            memcpy(code + 10, "\x89\xC2\x48\x89\xF9\x41\xB8", 7);
            memcpy(code + 17, &which, 4);
            memcpy(code + 21, "\x48\xB8", 2);
            memcpy(code + 23, &fn, 8);
            memcpy(code + 31, "\xFF\xD0\x48\x83\xC4\x28\xC3", 7);
            map_stub[i] = emit(code, 38);
        }
        for (i = 0; i < MARK_COUNT; i++) {
            if (!g_marks[i].call)
                continue;
            /* sub rsp,28 / call compare / add rsp,28 / test eax,eax / je +7 /
             * or dword [rsi+0x70],bit / ret */
            mark_stub[i] = emit((const unsigned char *)"\x48\x83\xEC\x28\xE8\0\0\0\0\x48\x83\xC4\x28"
                                "\x85\xC0\x74\x07\x81\x4E\x70\0\0\0\0\xC3", 25);
            if (!mark_stub[i] || !rel32(mark_stub[i] + 9, compare, &r)) {
                mark_stub[i] = NULL;
                continue;
            }
            memcpy(mark_stub[i] + 5, &r, 4);
            memcpy(mark_stub[i] + 20, &g_marks[i].bit, 4);
        }
        VirtualProtect(g_page, 4096, PAGE_EXECUTE_READ, &old);
        FlushInstructionCache(GetCurrentProcess(), g_page, 4096);

        /* 4. switch the game over to them */
        if (spare_ok) {
            unsigned char zero = 0;
            if (write_code(reset + RESET_MASK_TOP, &zero, 1)) {
                log_line("skill code: status reset now clears spare bits 25-31 (game+0x%llX)",
                         (unsigned long long)(reset - base));
                for (i = 0; i < MARK_COUNT; i++) {
                    if (!mark_stub[i] || !rel32(g_marks[i].call + 5, mark_stub[i], &r))
                        continue;
                    code[0] = 0xE8;
                    memcpy(code + 1, &r, 4);
                    if (write_code(g_marks[i].call, code, 5)) {
                        marks_done++;
                        log_line("skill code: %s now marks the fighter (game+0x%llX)", g_marks[i].decal,
                                 (unsigned long long)(g_marks[i].call - base));
                    }
                }
            } else {
                log_line("skill code: could not change the status reset - only Treasure Hunter will work");
                g_checks[0].flags = FLAG_SEARCHUP_ITEM;
                for (i = 1; i < CHECK_COUNT; i++)
                    map_stub[i] = NULL;
            }
        }
        for (i = 0; i < CHECK_COUNT; i++) {
            if (!map_stub[i] || !rel32(g_checks[i].call + 5, map_stub[i], &r))
                continue;
            code[0] = 0xE8;             /* call stub / nop */
            memcpy(code + 1, &r, 4);
            code[5] = 0x90;
            if (write_code(g_checks[i].call, code, 6)) {
                checks_done++;
                log_line("pause map, %s: check now accepts the fighter passive (game+0x%llX)",
                         g_checks[i].what, (unsigned long long)(g_checks[i].call - base));
            }
        }
    }
    log_line("ready: %d of %d pause-map checks, %d of %d decals marked",
             checks_done, (int)CHECK_COUNT, marks_done, (int)MARK_COUNT);
}

/* ---------------------------------------------------------------- start */

static int running_in_the_game(void)
{
    wchar_t exe[MAX_PATH];
    const wchar_t *name;
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);

    if (n == 0 || n >= MAX_PATH)
        return 0;
    name = wcsrchr(exe, L'\\');
    name = name ? name + 1 : exe;
    return _wcsicmp(name, L"BrgGame-Steam.exe") == 0;
}

/* Earlier builds of this plugin were called lid-fighter.dll. With both
 * in lid-plugins the two would make the same changes, so the new name gives
 * way to the old one until it is deleted. */
static int old_name_beside_me(void)
{
    wchar_t old[MAX_PATH];
    wchar_t *slash;

    if (!g_log_path[0] || wcslen(g_log_path) >= MAX_PATH)
        return 0;
    wcscpy(old, g_log_path);
    slash = wcsrchr(old, L'\\');
    if (!slash || (size_t)(slash - old) + 17 >= MAX_PATH)
        return 0;
    wcscpy(slash + 1, L"lid-fighter.dll");
    return GetFileAttributesW(old) != INVALID_FILE_ATTRIBUTES;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        wchar_t *slash;
        DWORD n;

        DisableThreadLibraryCalls(inst);
        n = GetModuleFileNameW(inst, g_log_path, MAX_PATH);
        slash = (n && n < MAX_PATH - 32) ? wcsrchr(g_log_path, L'\\') : NULL;
        if (slash)
            wcscpy(slash + 1, L"lid-classpassives.log");
        else
            g_log_path[0] = 0;

        if (!running_in_the_game())
            return TRUE;    /* some other program - change nothing */
        log_line("LiD Fighter Class Passives lid-classpassives.dll " LID_DLL_VERSION " loaded");
        if (old_name_beside_me()) {
            log_line("lid-fighter.dll (this plugin's old name) is in the same folder and makes the "
                     "same changes, so this copy stays out of its way - nothing changed. Delete "
                     "lid-fighter.dll and this one takes over.");
            return TRUE;
        }
        patch_game();
    }
    (void)reserved;
    return TRUE;
}
