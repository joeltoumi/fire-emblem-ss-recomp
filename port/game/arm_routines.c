// fire-emblem-ss-recomp — C versions of the ARM routines in asm/arm.s.
//
// On the GBA these run from IWRAM: StoreRoutinesToIRAM copies them there and
// sets the function pointers the game calls them through (src/ramfunc.c),
// and asm/arm_call.s has Thumb entry points for the others. Each function
// follows its assembly exactly, including loop counts and quirks (noted
// where the comments in asm/arm.s say otherwise).
#include "global.h"
#include "hardware.h"
#include "bmmap.h"
#include "bmidoten.h"

extern u16 gPaletteBuffer[];
extern const u32 gMsgHuffmanTable[];
extern const u32 gMsgHuffmanTableRoot; // ROM: GBA address of the root node

// ---- palette fade -----------------------------------------------------------

static void ColorFadeTick(void)
{
    int pal, color;

    for (pal = 0x1F; pal >= 0; pal--) {
        int step = gFadeComponentStep[pal];
        u8 *comp;

        if (step == 0)
            continue;
        comp = (u8 *)gFadeComponents + 0x30 + pal * 0x30;
        for (color = 0xF; color >= 0; color--) {
            int rgb[3];
            int i;

            comp -= 3;
            for (i = 0; i < 3; i++) {
                int c;

                comp[i] = (u8)(comp[i] + step);
                c = comp[i] - 0x20;
                if (c < 0)
                    c = 0;
                if ((unsigned)c >= 0x20)
                    c = 0x1F;
                rgb[i] = c;
            }
            gPaletteBuffer[pal * 0x10 + color] = rgb[0] + (rgb[1] << 5) + (rgb[2] << 10);
        }
    }
}

void CALLARM_ColorFadeTick(void)
{
    ColorFadeTick();
}

// ---- OAM / tilemaps ---------------------------------------------------------

// Moves `count` OBJs off screen (Y = 160). Always does at least 16.
void ClearOAMBuffer(void *buf, int count)
{
    u32 *dst = buf;
    int n = count / 16 - 1;

    // Before InitOam the buffer is NULL: on the GBA these writes hit the
    // BIOS and are ignored.
    if (dst == NULL)
        return;

    do {
        int i;
        for (i = 0; i < 16; i++)
            dst[i * 2] = 160;
        dst += 32;
    } while (--n >= 0);
}

// Sum and XOR of the halfwords (`size` bytes).
u32 ComputeChecksum32(const u32 *buf32, int size)
{
    const u16 *buf = (const u16 *)buf32;
    u32 add = 0, xor = 0;
    int n = size - 2;

    do {
        add += *buf;
        xor ^= *buf;
        buf++;
        n -= 2;
    } while (n >= 0);
    return (add & 0xFFFF) + (xor << 16);
}

// Fills (width + 1) x (height + 1) entries of a 32-wide tilemap.
void TileMap_FillRect(u16 *dest, int width, int height, int fillValue)
{
    int h = height;

    do {
        u16 *row = dest;
        int w = width;
        do {
            *row++ = fillValue;
        } while (--w >= 0);
        dest += 0x20;
    } while (--h >= 0);
}

void TileMap_CopyRect(u16 *src, u16 *dst, int width, int height)
{
    int h;

    if (width <= 0 || height <= 0)
        return;
    for (h = height - 1; h >= 0; h--) {
        int w;
        for (w = width - 1; w >= 0; w--)
            *dst++ = *src++;
        src += 0x20 - width;
        dst += 0x20 - width;
    }
}

// Writes a TSA (u8 width - 1, u8 height - 1, then the entries bottom row
// first) into a 32-wide tilemap, adding `tileref` to each entry.
void CallARM_FillTileRect(u16 *tilemap, const void *tsa, int tileref)
{
    const u8 *hdr = tsa;
    int width = hdr[0];
    int height = hdr[1];
    const u16 *src = (const u16 *)(hdr + 2);
    u16 *dst = tilemap + height * 0x20;
    int h;

    for (h = height; h >= 0; h--) {
        int w;
        for (w = width; w >= 0; w--)
            *dst++ = *src++ + tileref;
        dst -= width + 1 + 0x20;
    }
}

// Appends the OBJs of an OAM list (count, then 3 attributes each) at the
// put pointer, moved by (x, y). The high bits of y are ORed into attribute 0
// and those of x into attribute 1, as in the original.
static void PutOam(u16 **pdst, int x, int y, const u16 *list, int oam2)
{
    u16 *dst = *pdst;
    int count = list[0];
    u32 xy;

    if (count == 0)
        return;
    list++;
    *pdst = dst + count * 4;
    xy = (x & 0xFFFF) | ((u32)(y & 0xFFFF) << 16);
    do {
        u32 a = list[0];
        dst[0] = ((a | (xy >> 16)) & 0xFF00) | ((a + (xy >> 16)) & 0xFF);
        a = list[1];
        dst[1] = ((a | xy) & 0xFE00) | (((a + xy) << 23) >> 23);
        dst[2] = list[2] + oam2;
        list += 3;
        dst += 4;
    } while (--count != 0);
}

void Port_PutOamHi(int x, int y, const u16 *list, int oam2)
{
    PutOam((u16 **)&gOamHiPutIt, x, y, list, oam2);
}

void Port_PutOamLo(int x, int y, const u16 *list, int oam2)
{
    PutOam((u16 **)&gOamLoPutIt, x, y, list, oam2);
}

// ---- text -------------------------------------------------------------------

static const u32 sBitTable[8] = {
    1 << 0, 1 << 2, 1 << 4, 1 << 6, 1 << 8, 1 << 10, 1 << 12, 1 << 14,
};

// Two pixels-to-4bpp lookups (`pal` is a 256-entry u16 table) of the low and
// high byte of a halfword.
static inline u32 Expand(const u16 *pal, u32 bits)
{
    return pal[bits & 0xFF] | ((u32)pal[(bits >> 8) & 0xFF] << 16);
}

// ORs a 16-row 2bpp glyph, shifted right by `subx` pixels, into three
// columns of 4bpp tiles (0x40 bytes apart).
void Port_DrawGlyph(int pal_, int dst_, int src_, int subx)
{
    const u16 *pal = (const u16 *)(uintptr_t)(u32)pal_;
    u32 *dst = (u32 *)(uintptr_t)(u32)dst_;
    const u32 *src = (const u32 *)(uintptr_t)(u32)src_;
    int row;

    for (row = 15; row >= 0; row--) {
        u64 v = (u64)*src * sBitTable[subx];
        u32 lo = (u32)v, hi = (u32)(v >> 32);

        dst[0x00] |= Expand(pal, lo);
        dst[0x10] |= Expand(pal, lo >> 16);
        dst[0x20] |= Expand(pal, hi);
        dst++;
        src++;
    }
}

// The 8-row version (not called through a pointer). Reads the second and
// third columns 0x40 / 0x80 bytes away but writes them 0x20 / 0x40 away.
void sub_8000620(const u16 *pal, u32 *dst, const u32 *src, int subx)
{
    int row;

    for (row = 7; row >= 0; row--) {
        u64 v = (u64)*src * sBitTable[subx];
        u32 lo = (u32)v, hi = (u32)(v >> 32);

        dst[0x00] |= Expand(pal, lo);
        dst[0x08] = dst[0x10] | Expand(pal, lo >> 16);
        dst[0x10] = dst[0x20] | Expand(pal, hi);
        dst++;
        src++;
    }
}

// Huffman decoding of a message. Internal nodes are two u16 child indices,
// leaves have bit 31 set and hold one or two characters; 0 ends the string.
void Port_DecodeString(const char *src_, char *dst_)
{
    const u8 *src = (const u8 *)src_;
    u8 *dst = (u8 *)dst_;
    const u32 *root = (const u32 *)(uintptr_t)gMsgHuffmanTableRoot;
    int bits = 0;
    u32 cur = 0;

    for (;;) {
        const u32 *node = root;
        u32 leaf;

        for (;;) {
            u16 next;

            if (--bits < 0) {
                cur = *src++;
                bits = 7;
            }
            next = (cur & 1) ? ((const u16 *)node)[1] : ((const u16 *)node)[0];
            node = gMsgHuffmanTable + next;
            cur >>= 1;
            leaf = *node;
            if ((s32)leaf < 0)
                break;
        }
        if (leaf & 0xFF00) {
            dst[0] = leaf;
            dst[1] = leaf >> 8;
            dst += 2;
            continue;
        }
        dst[0] = leaf;
        if ((leaf & 0xFF) == 0)
            return;
        dst++;
    }
}

// ---- movement maps ----------------------------------------------------------

void Port_MapFloodCoreStep(int connexion, int xOff, int yOff)
{
    struct MovMapFillState *st = &gMovMapFillState;
    int xsrc = st->src->xPos;
    int ysrc = st->src->yPos;
    int x = xOff + xsrc;
    int y = yOff + ysrc;
    u32 cost = gWorkingTerrainMoveCosts[gBmMapTerrain[y][x]] + gWorkingBmMap[ysrc][xsrc];

    if (cost >= gWorkingBmMap[y][x])
        return;
    if (st->hasUnit) {
        u8 uid = gBmMapUnit[y][x];
        if (uid != 0 && ((st->unitId ^ uid) & 0x80))
            return;
    }
    if (cost > st->movement)
        return;
    st->dst->xPos = x;
    st->dst->yPos = y;
    st->dst->connexion = connexion;
    st->dst->leastMoveCost = cost;
    st->dst++;
    gWorkingBmMap[y][x] = cost;
}

void Port_MapFloodCore(void)
{
    int i = 0;

    for (;;) {
        i ^= 1;
        if (i) {
            gMovMapFillState.src = gMovMapFillStPool1;
            gMovMapFillState.dst = gMovMapFillStPool2;
        } else {
            gMovMapFillState.src = gMovMapFillStPool2;
            gMovMapFillState.dst = gMovMapFillStPool1;
        }
        if (gMovMapFillState.src->connexion == 4)
            return;

        for (;;) {
            switch (gMovMapFillState.src->connexion) {
            case 0:
                Port_MapFloodCoreStep(3, 0, -1);
                Port_MapFloodCoreStep(2, 0, 1);
                Port_MapFloodCoreStep(0, -1, 0);
                break;
            case 1:
                Port_MapFloodCoreStep(3, 0, -1);
                Port_MapFloodCoreStep(2, 0, 1);
                Port_MapFloodCoreStep(1, 1, 0);
                break;
            case 2:
                Port_MapFloodCoreStep(2, 0, 1);
                Port_MapFloodCoreStep(0, -1, 0);
                Port_MapFloodCoreStep(1, 1, 0);
                break;
            case 3:
                Port_MapFloodCoreStep(3, 0, -1);
                Port_MapFloodCoreStep(0, -1, 0);
                Port_MapFloodCoreStep(1, 1, 0);
                break;
            case 4:
                goto next_pass;
            default: // 5: the starting square
                Port_MapFloodCoreStep(3, 0, -1);
                Port_MapFloodCoreStep(2, 0, 1);
                Port_MapFloodCoreStep(0, -1, 0);
                Port_MapFloodCoreStep(1, 1, 0);
                break;
            }
            gMovMapFillState.dst->connexion = 4;
            gMovMapFillState.src++;
        }
    next_pass:;
    }
}
