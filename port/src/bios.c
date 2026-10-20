// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — replacements for the GBA BIOS calls
// (src/libagbsyscall.s).
#include "port.h"

#include <math.h>

// ---- memory -----------------------------------------------------------------

// CpuSet: bits 0-20 = unit count, bit 24 = fill (fixed source), bit 26 = 32-bit.
void CpuSet(const void* src, void* dst, uint32_t control)
{
    const uint32_t count = control & 0x1FFFFF;
    const bool fill = (control >> 24) & 1;
    const uint32_t unit = ((control >> 26) & 1) ? 4 : 2;
    const uint8_t* s = (const uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;

    if (!fill) {
        memmove(d, s, (size_t)count * unit);
        return;
    }
    for (uint32_t i = 0; i < count; i++)
        memcpy(d + (size_t)i * unit, s, unit);
}

// CpuFastSet: 32-bit units, count rounded up to a multiple of 8 words.
void CpuFastSet(const void* src, void* dst, uint32_t control)
{
    const uint32_t count = ((control & 0x1FFFFF) + 7) & ~7u;
    const bool fill = (control >> 24) & 1;
    const uint8_t* s = (const uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;

    if (!fill) {
        memmove(d, s, (size_t)count * 4);
        return;
    }
    for (uint32_t i = 0; i < count; i++)
        memcpy(d + (size_t)i * 4, s, 4);
}

void RegisterRamReset(uint32_t flags)
{
    if (flags & 0x01)
        memset(gPortEwram, 0, sizeof(gPortEwram));
    if (flags & 0x02) // all but the top 0x200 bytes (stacks, BIOS words)
        memset(gPortIwram, 0, sizeof(gPortIwram) - 0x200);
    if (flags & 0x04)
        memset(gPortPalram, 0, sizeof(gPortPalram));
    if (flags & 0x08)
        memset(gPortVram, 0, sizeof(gPortVram));
    if (flags & 0x10)
        memset(gPortOam, 0, sizeof(gPortOam));
    if (flags & 0x80) { // other I/O registers (sound, serial excluded)
        memset(gPortIo, 0, 0x60);
        memset(gPortIo + 0xB0, 0, 0x120 - 0xB0);
        memset(gPortIo + 0x200, 0, 0x200);
        Port_IoWrite16(IO_KEYINPUT, 0x03FF);
    }
}

// ---- math -------------------------------------------------------------------

int32_t Div(int32_t number, int32_t denom)
{
    if (denom == 0)
        return number < 0 ? -1 : 1;
    if (number == INT32_MIN && denom == -1)
        return INT32_MIN;
    return number / denom;
}

int32_t DivRem(int32_t number, int32_t denom)
{
    if (denom == 0 || (number == INT32_MIN && denom == -1))
        return 0;
    return number % denom;
}

// SWI 7 takes its arguments the other way round: (denom, number).
int32_t DivArm(int32_t denom, int32_t number)
{
    return Div(number, denom);
}

uint16_t Sqrt(uint32_t number)
{
    uint32_t r = (uint32_t)sqrt((double)number);
    // Guard against floating point rounding at the boundaries.
    while ((uint64_t)r * r > number)
        r--;
    while ((uint64_t)(r + 1) * (r + 1) <= number)
        r++;
    return (uint16_t)r;
}

// The BIOS arctangent: a polynomial in fixed point (same steps as the BIOS).
static int32_t ArcTanPoly(int32_t i)
{
    int32_t a = -((i * i) >> 14);
    int32_t b = ((0xA9 * a) >> 14) + 0x390;

    b = ((b * a) >> 14) + 0x91C;
    b = ((b * a) >> 14) + 0xFB6;
    b = ((b * a) >> 14) + 0x16AA;
    b = ((b * a) >> 14) + 0x2081;
    b = ((b * a) >> 14) + 0x3651;
    b = ((b * a) >> 14) + 0xA2F9;
    return (i * b) >> 16;
}

uint16_t ArcTan2(int16_t x16, int16_t y16)
{
    int32_t x = x16, y = y16;

    if (y == 0)
        return x >= 0 ? 0 : 0x8000;
    if (x == 0)
        return y >= 0 ? 0x4000 : 0xC000;
    if (y >= 0) {
        if (x >= 0) {
            if (x >= y)
                return (uint16_t)ArcTanPoly((y << 14) / x);
        } else if (-x >= y) {
            return (uint16_t)(ArcTanPoly((y << 14) / x) + 0x8000);
        }
        return (uint16_t)(0x4000 - ArcTanPoly((x << 14) / y));
    }
    if (x <= 0) {
        if (-x > -y)
            return (uint16_t)(ArcTanPoly((y << 14) / x) + 0x8000);
    } else if (x >= -y) {
        return (uint16_t)(ArcTanPoly((y << 14) / x) + 0x10000);
    }
    return (uint16_t)(0xC000 - ArcTanPoly((x << 14) / y));
}

struct PortBgAffineSrc {
    int32_t texX, texY;
    int16_t scrX, scrY;
    int16_t sx, sy;
    uint16_t alpha;
};

struct PortBgAffineDst {
    int16_t pa, pb, pc, pd;
    int32_t dx, dy;
};

void BgAffineSet(const struct PortBgAffineSrc* src, struct PortBgAffineDst* dst, int32_t count)
{
    for (; count > 0; count--, src++, dst++) {
        const float ox = src->texX / 256.f, oy = src->texY / 256.f;
        const float cx = src->scrX, cy = src->scrY;
        const float sx = src->sx / 256.f, sy = src->sy / 256.f;
        const float theta = (src->alpha >> 8) / 128.f * (float)M_PI;
        const float a = cosf(theta) * sx, b = -sinf(theta) * sx;
        const float c = sinf(theta) * sy, d = cosf(theta) * sy;

        dst->pa = (int16_t)(a * 256);
        dst->pb = (int16_t)(b * 256);
        dst->pc = (int16_t)(c * 256);
        dst->pd = (int16_t)(d * 256);
        dst->dx = (int32_t)((ox - (a * cx + b * cy)) * 256);
        dst->dy = (int32_t)((oy - (c * cx + d * cy)) * 256);
    }
}

struct PortObjAffineSrc {
    int16_t sx, sy;
    uint16_t alpha;
    uint16_t pad;
};

void ObjAffineSet(const struct PortObjAffineSrc* src, void* dst, int32_t count, int32_t offset)
{
    uint8_t* d = dst;

    for (; count > 0; count--, src++) {
        const float sx = src->sx / 256.f, sy = src->sy / 256.f;
        const float theta = (src->alpha >> 8) / 128.f * (float)M_PI;
        const int16_t m[4] = {
            (int16_t)(cosf(theta) * sx * 256), (int16_t)(-sinf(theta) * sx * 256),
            (int16_t)(sinf(theta) * sy * 256), (int16_t)(cosf(theta) * sy * 256),
        };
        for (int i = 0; i < 4; i++)
            memcpy(d + i * offset, &m[i], 2);
        d += offset * 4;
    }
}

// ---- decompression ----------------------------------------------------------

static void Lz77UnComp(const void* src, void* dst)
{
    const uint8_t* s = (const uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;
    uint32_t header;
    uint32_t size;
    uint32_t written = 0;

    memcpy(&header, s, 4);
    s += 4;
    if ((header & 0xF0) != 0x10) {
        Port_Log("LZ77: bad header %08X at %p", header, src);
        return;
    }
    size = header >> 8;

    while (written < size) {
        uint8_t flags = *s++;
        for (int bit = 0; bit < 8 && written < size; bit++, flags <<= 1) {
            if (flags & 0x80) {
                uint32_t len = (uint32_t)(s[0] >> 4) + 3;
                uint32_t disp = (((uint32_t)(s[0] & 0xF) << 8) | s[1]) + 1;
                s += 2;
                while (len-- > 0 && written < size) {
                    d[written] = d[written - disp];
                    written++;
                }
            } else {
                d[written++] = *s++;
            }
        }
    }
}

void LZ77UnCompWram(const void* src, void* dst)
{
    Lz77UnComp(src, dst);
}

void LZ77UnCompVram(const void* src, void* dst)
{
    // VRAM only takes 16-bit writes on hardware; the byte-wise result is
    // the same except for an odd trailing byte.
    Lz77UnComp(src, dst);
}

static void RlUnComp(const void* src, void* dst)
{
    const uint8_t* s = (const uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;
    uint32_t header;
    uint32_t size;
    uint32_t written = 0;

    memcpy(&header, s, 4);
    s += 4;
    size = header >> 8;
    while (written < size) {
        uint8_t flag = *s++;
        if (flag & 0x80) {
            uint32_t len = (flag & 0x7F) + 3;
            uint8_t v = *s++;
            while (len-- > 0 && written < size)
                d[written++] = v;
        } else {
            uint32_t len = (flag & 0x7F) + 1;
            while (len-- > 0 && written < size)
                d[written++] = *s++;
        }
    }
}

void RLUnCompWram(const void* src, void* dst)
{
    RlUnComp(src, dst);
}

void RLUnCompVram(const void* src, void* dst)
{
    RlUnComp(src, dst);
}

// Huffman: header (bits per symbol in the low nibble, size), tree size byte,
// tree, then the bitstream in 32-bit words read from the top bit.
void HuffUnComp(const void* src, void* dst)
{
    const uint8_t* s = (const uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;
    uint32_t header;
    memcpy(&header, s, 4);
    const uint32_t bitsPerSym = header & 0xF;
    const uint32_t size = header >> 8;
    const uint8_t* tree = s + 4;
    const uint8_t* root = tree + 1;
    const uint8_t* stream = tree + (tree[0] + 1) * 2;
    uint32_t written = 0;
    uint32_t outWord = 0;
    uint32_t outBits = 0;
    const uint8_t* node = root;

    if (bitsPerSym != 4 && bitsPerSym != 8) {
        Port_Log("HuffUnComp: unsupported %u bits per symbol", bitsPerSym);
        return;
    }
    while (written < size) {
        uint32_t word;
        memcpy(&word, stream, 4);
        stream += 4;
        for (int bit = 31; bit >= 0 && written < size; bit--) {
            const uint8_t n = *node;
            const int right = (word >> bit) & 1;
            const uint8_t* child = (const uint8_t*)(((uintptr_t)node & ~(uintptr_t)1) + (n & 0x3F) * 2 + 2 + right);
            const bool isLeaf = right ? (n & 0x40) : (n & 0x80);

            if (!isLeaf) {
                node = child;
                continue;
            }
            outWord |= (uint32_t)*child << outBits;
            outBits += bitsPerSym;
            node = root;
            if (outBits == 32) {
                memcpy(d + written, &outWord, 4);
                written += 4;
                outWord = 0;
                outBits = 0;
            }
        }
    }
}

// ---- sound ------------------------------------------------------------------

void SoundBiasReset(void)
{
}

void SoundBiasSet(void)
{
}

// ---- misc -------------------------------------------------------------------

int MultiBoot(void* mbp)
{
    (void)mbp;
    return 1; // failure: no link cable
}

void VBlankIntrWait(void)
{
    Port_RunFrame();
}

_Noreturn void SoftReset(uint32_t flags)
{
    (void)flags;
    Port_SoftReset();
}
