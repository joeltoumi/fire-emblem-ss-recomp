// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — the GBA memory map, at its hardware addresses.
//
// Every region is a host mapping at the address it has on the GBA, and the
// game image is loaded below 4 GiB too (see docs/PORT.md). The gPort* region
// symbols are absolute symbols at those addresses
// (port/data/hardware_symbols.txt).
#include "port.h"

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

typedef struct Region {
    const char* name;
    uintptr_t address;
    size_t size;
} Region;

// Sizes are rounded up to the host page size. IWRAM gets room past its end:
// the BIOS words at its top are read as 32-bit values next to the edge.
static const Region sRegions[] = {
    { "EWRAM", 0x02000000, 256 * 0x400 },
    { "IWRAM", 0x03000000, 64 * 0x400 },
    { "I/O", 0x04000000, 0x400 },
    { "palette", 0x05000000, 0x400 },
    { "VRAM", 0x06000000, 96 * 0x400 },
    { "OAM", 0x07000000, 0x400 },
    { "ROM", 0x08000000, 32 * 0x400 * 0x400 },
    { "SRAM", 0x0E000000, 64 * 0x400 },
};

void* gPortSoundInfoPtr;
unsigned short gPortIntrCheck;
void* gPortIntrVector;

static size_t PageRound(size_t size)
{
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    return (size + page - 1) / page * page;
}

bool Port_MapGbaMemory(void)
{
    static bool sMapped;

    if (sMapped)
        return true;
    for (size_t i = 0; i < sizeof(sRegions) / sizeof(sRegions[0]); i++) {
        const Region* r = &sRegions[i];
        void* want = (void*)r->address;
        void* got = mmap(want, PageRound(r->size), PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (got != want) {
            Port_Log("cannot map the GBA %s at %p (%s)", r->name, want,
                got == MAP_FAILED ? strerror(errno) : "address taken");
            if (got != MAP_FAILED)
                munmap(got, PageRound(r->size));
            return false;
        }
    }
    sMapped = true;
    return true;
}

// The ROM region is read-only once the player's ROM is in it.
void Port_ProtectRom(bool readOnly)
{
    mprotect(gPortRom, sizeof(gPortRom), readOnly ? PROT_READ : PROT_READ | PROT_WRITE);
}

void Port_MemReset(void)
{
    memset(gPortEwram, 0, sizeof(gPortEwram));
    memset(gPortIwram, 0, sizeof(gPortIwram));
    memset(gPortIo, 0, sizeof(gPortIo));
    memset(gPortPalram, 0, sizeof(gPortPalram));
    memset(gPortVram, 0, sizeof(gPortVram));
    memset(gPortOam, 0, sizeof(gPortOam));
    gPortSoundInfoPtr = NULL;
    gPortIntrCheck = 0;
    gPortIntrVector = NULL;
    // Unpressed keys read as 1 (active-low KEYINPUT).
    Port_IoWrite16(0x130, 0x03FF);
}
