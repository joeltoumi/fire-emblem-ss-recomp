// The game side of the widescreen (include/port/widescreen.h): the whole
// battle map as tilemaps for the extra columns of a wide frame, and which OAM
// entries are units or weather.
//
// Nothing here changes what the game does. The terrain (BG3) is rebuilt from
// the map the same way DisplayBmTile draws it and the movement/attack range
// (BG2) the same way DisplayMovementViewTile does. Both are compared with
// what the game's own ring tilemaps show on the GBA screen: when they don't
// match (a battle scene, a menu or a cutscene over the map), the frame is
// drawn at 240 px like any other.
#include "global.h"
#include "proc.h"
#include "bm.h"
#include "bmmap.h"
#include "port/widescreen.h"

// The tilemaps and tags are rebuilt for every frame: not machine state, so
// they stay out of save states (include/port/native.h).
#pragma clang section bss = "" data = ""

extern struct ProcCmd gProc_BMapMain[];

const u16 *Port_BmTilesetConfig(void);

#define MAX_MAP 64 // tiles of 16 px
#define MAX_TILES (MAX_MAP * 2)

int gPortWideOamTag;

static u16 sTerrain[MAX_TILES * MAX_TILES];
static u16 sRange[MAX_TILES * MAX_TILES];
static u8 sBufTags[128]; // the OAM buffer being filled
static u8 sTags[128];    // the OAM being shown

void Port_WideTagOam(const void *entry, int count)
{
    int i = ((const u8 *)entry - (const u8 *)gOam) / 8;

    for (; count > 0 && i < 128; count--, i++)
        if (i >= 0)
            sBufTags[i] = gPortWideOamTag;
}

void Port_WideOamSynced(const void *buffer, int count)
{
    int i = ((const u8 *)buffer - (const u8 *)gOam) / 8;

    for (; count > 0 && i < 128; count--, i++) {
        if (i < 0)
            continue;
        sTags[i] = sBufTags[i];
        sBufTags[i] = PORT_WIDE_OAM_SCREEN;
    }
}

static void BuildTerrain(int w, int h)
{
    const u16 *config = Port_BmTilesetConfig();
    int x, y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            const u16 *tile = config + gBmMapBaseTiles[y][x];
            u16 base = gBmMapFog[y][x] ? (6 << 12) : (11 << 12);
            u16 *out = sTerrain + (y * 2) * (w * 2) + x * 2;

            out[0] = base + tile[0];
            out[1] = base + tile[1];
            out[w * 2] = base + tile[2];
            out[w * 2 + 1] = base + tile[3];
        }
    }
}

static void BuildRange(int w, int h)
{
    int x, y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            u16 *out = sRange + (y * 2) * (w * 2) + x * 2;
            u16 tile = 0;

            if (gMapMovementSigned[y][x] >= 0)
                tile = 0x4280;
            else if (gMapRangeSigned[y][x])
                tile = 0x5280;

            out[0] = tile ? tile + 0 : 0;
            out[1] = tile ? tile + 1 : 0;
            out[w * 2] = tile ? tile + 2 : 0;
            out[w * 2 + 1] = tile ? tile + 3 : 0;
        }
    }
}

// DisplayMovementViewTile draws attack range over a stale tile with the
// 0x5284 set instead of 0x5280: same thing for the comparison.
static u16 Normalize(int bg, u16 entry)
{
    if (bg == 2 && (entry & 0xFFFC) == 0x5284)
        return entry - 4;
    return entry;
}

// Places a map tilemap under BG `bg` the way its ring is scrolled, and
// checks that the ring shows the same thing on the GBA screen. Returns FALSE
// when it doesn't.
static bool8 PlaceBg(struct PortWideBg *out, int bg, const u16 *map, int w, int h)
{
    const u16 cnt = *(vu16 *)(REG_ADDR_BG0CNT + bg * 2);
    const u16 *ring = (const u16 *)(VRAM + ((cnt >> 8) & 0x1F) * 0x800);
    const int hofs = *(vu16 *)(REG_ADDR_BG0HOFS + bg * 4) & 0x1FF;
    const int vofs = *(vu16 *)(REG_ADDR_BG0VOFS + bg * 4) & 0x1FF;
    int x, y, worldX, worldY, bad = 0, total = 0;

    if (cnt & 0xC080) // a 32x32 4bpp map, like the game's
        return FALSE;

    // Ring pixel p shows map pixel p + mapRenderOrigin * 16 (mod 256); take
    // the one closest to the camera.
    worldX = hofs + gBmSt.mapRenderOrigin.x * 16;
    worldX += ((gBmSt.camera.x - worldX + 128 + 256 * 64) & ~255) - 256 * 64;
    worldY = vofs + gBmSt.mapRenderOrigin.y * 16;
    worldY += ((gBmSt.camera.y - worldY + 128 + 256 * 64) & ~255) - 256 * 64;

    for (y = 0; y < DISPLAY_HEIGHT; y += 8) {
        int my = worldY + y;
        int ry = ((vofs + y) & 255) >> 3;

        if (my < 0 || my >= h * 8)
            continue;
        for (x = 0; x < DISPLAY_WIDTH; x += 8) {
            int mx = worldX + x;
            int rx = ((hofs + x) & 255) >> 3;

            if (mx < 0 || mx >= w * 8)
                continue;
            total++;
            if (Normalize(bg, ring[ry * 32 + rx]) != Normalize(bg, map[(my >> 3) * w + (mx >> 3)]))
                bad++;
        }
    }
    // Allow a few tiles the game is still redrawing.
    if (total == 0 || bad * 10 > total)
        return FALSE;

    out->map = map;
    out->w = w;
    out->h = h;
    out->x = worldX;
    out->y = worldY;
    out->regX = hofs;
    out->regY = vofs;
    return TRUE;
}

void Port_WideGameFrame(void)
{
    static struct PortWideFrame frame;
    const u16 dispcnt = *(vu16 *)REG_ADDR_DISPCNT;
    int w = gBmMapSize.x, h = gBmMapSize.y;

    if ((dispcnt & 7) != 0 || !(dispcnt & DISPCNT_BG3_ON) || (dispcnt & DISPCNT_FORCED_BLANK))
        return;
    if (Proc_Find(gProc_BMapMain) == NULL || !gBmMapBaseTiles || !gBmMapFog || !gBmMapMovement ||
        !gBmMapRange || w <= 0 || h <= 0 || w > MAX_MAP || h > MAX_MAP)
        return;

    memset(&frame, 0, sizeof(frame));
    BuildTerrain(w, h);
    if (!PlaceBg(&frame.bg[3], 3, sTerrain, w * 2, h * 2))
        return;
    if ((gBmSt.gameStateBits & BM_FLAG_0) && (dispcnt & DISPCNT_BG2_ON)) {
        BuildRange(w, h);
        PlaceBg(&frame.bg[2], 2, sRange, w * 2, h * 2);
    }

    memcpy(frame.oamTags, sTags, sizeof(frame.oamTags));
    frame.cameraX = frame.bg[3].x;
    frame.left = 0;
    frame.right = w * 16;
    Port_WideSetFrame(&frame);
}
