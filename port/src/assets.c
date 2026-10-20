// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — the player's ROM.
//
// The build contains no game data: everything the decomp builds from data
// files (graphics, maps, text, music...) is an absolute symbol at its ROM
// address, and the game reads it in place from a copy of the player's ROM
// mapped at the cartridge address, 0x08000000 (port/src/gba_mem.c).
// Port_LoadRom() checks the ROM is the exact dump this build expects
// (SHA-1) and puts it there.
#include "port.h"
#include "port_assets.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef FE8_ROM_SHA1
#error "FE8_ROM_SHA1 must be defined by the build (expected SHA-1 of the ROM)"
#endif

static size_t sRomSize;

const uint8_t* Port_RomData(size_t* size)
{
    *size = sRomSize;
    return gPortRom;
}

const char* Port_RomRegionName(void)
{
    return FE8_ROM_REGION_NAME;
}

const char* Port_RomExpectedSha1(void)
{
    return FE8_ROM_SHA1;
}

PortRomStatus Port_CheckRom(const uint8_t* rom, size_t size)
{
    char hex[41];
    uint8_t digest[20];

    if (size < 0x200 || size > sizeof(gPortRom))
        return PORT_ROM_BAD_SIZE;
    // Development/CI only: boot with any image (e.g. a zero-filled dummy) to
    // smoke-test the engine without game data.
    if (getenv("FE8_DEV_SKIP_ROM_CHECK") != NULL) {
        Port_Log("FE8_DEV_SKIP_ROM_CHECK set: not verifying the ROM");
        return PORT_ROM_OK;
    }
    // Header: game title at 0xA0 ("FIREEMBLEM2E" in the US version).
    if (memcmp(rom + 0xA0, "FIREEMBLEM2", 11) != 0)
        return PORT_ROM_NOT_FE8;

    Port_Sha1(rom, size, digest);
    for (int i = 0; i < 20; i++)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    if (strcmp(hex, FE8_ROM_SHA1) != 0) {
        Port_Log("ROM SHA-1 %s, expected %s (%s)", hex, FE8_ROM_SHA1, FE8_ROM_REGION_NAME);
        return PORT_ROM_WRONG_VERSION;
    }
    return PORT_ROM_OK;
}

PortRomStatus Port_LoadRom(const uint8_t* rom, size_t size)
{
    PortRomStatus status = Port_CheckRom(rom, size);

    if (status != PORT_ROM_OK)
        return status;
    Port_ProtectRom(false);
    memcpy(gPortRom, rom, size);
    // Past the end of a smaller ROM, the GBA bus reads back the address.
    for (size_t off = size & ~(size_t)1; off < sizeof(gPortRom); off += 2) {
        const uint16_t v = (uint16_t)(off >> 1);
        memcpy(gPortRom + off, &v, 2);
    }
    Port_ProtectRom(true);
    sRomSize = size;
    Port_Log("ROM %s OK (%zu KiB)", FE8_ROM_REGION_NAME, size / 1024);
    return PORT_ROM_OK;
}

const char* Port_RomStatusText(PortRomStatus status)
{
    switch (status) {
    case PORT_ROM_OK:
        return "OK";
    case PORT_ROM_MISSING:
        return "No ROM selected";
    case PORT_ROM_BAD_SIZE:
        return "The file is not a GBA ROM";
    case PORT_ROM_NOT_FE8:
        return "The file is not Fire Emblem: The Sacred Stones";
    case PORT_ROM_WRONG_VERSION:
        return "Wrong version of Fire Emblem: The Sacred Stones (this build needs the " FE8_ROM_REGION_NAME " ROM)";
    }
    return "?";
}
