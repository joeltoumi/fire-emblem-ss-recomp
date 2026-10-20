// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — entry point (replaces asm/crt0.s).
#include "port.h"
#include "port_game.h"
#include "port_assets.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

static jmp_buf sExitJump;
static bool sQuit;
static bool sSramDirty;

void Port_RequestQuit(void)
{
    sQuit = true;
}

bool Port_QuitRequested(void)
{
    return sQuit;
}

_Noreturn void Port_ExitToMain(void)
{
    longjmp(sExitJump, 1);
}

void Port_SramWritten(void)
{
    sSramDirty = true;
}

void Port_FlushSave(void)
{
    if (!sSramDirty)
        return;
    Port_HostWriteSave(gPortSram, sizeof(gPortSram));
    sSramDirty = false;
}

void Port_Log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef __ANDROID__
    __android_log_vprint(ANDROID_LOG_INFO, "fe8", fmt, ap);
#else
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
#endif
    va_end(ap);
}

_Noreturn void Port_Fatal(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef __ANDROID__
    __android_log_vprint(ANDROID_LOG_FATAL, "fe8", fmt, ap);
#else
    fputs("FATAL: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
#endif
    va_end(ap);
    abort();
}

// The game's globals as they are at startup, restored on soft reset.
extern unsigned char __start_fe8_data[];
extern unsigned char __stop_fe8_data[];
extern unsigned char __start_fe8_bss[];
extern unsigned char __stop_fe8_bss[];
// EWRAM_DATA / IWRAM_DATA variables: GBA RAM the game never initializes,
// cleared at every start like the rest of RAM.
extern unsigned char __start_ewram_data[] __attribute__((weak));
extern unsigned char __stop_ewram_data[] __attribute__((weak));
extern unsigned char __start_iwram_data[] __attribute__((weak));
extern unsigned char __stop_iwram_data[] __attribute__((weak));
static unsigned char* sInitialData;

static jmp_buf sResetJump;

// Copies across a whole section, so across globals: AddressSanitizer would
// see each of these as an overflow of the first one.
__attribute__((no_sanitize("address"))) void Port_RawCopy(void* dst, const void* src, size_t size)
{
    unsigned char* d = dst;
    const unsigned char* s = src;
    while (size--)
        *d++ = *s++;
}

__attribute__((no_sanitize("address"))) void Port_RawZero(void* dst, size_t size)
{
    unsigned char* d = dst;
    while (size--)
        *d++ = 0;
}

_Noreturn void Port_SoftReset(void)
{
    longjmp(sResetJump, 1);
}

int Port_Main(int argc, char** argv)
{
    PortHostConfig cfg = { argc, argv };
    const size_t dataSize = (size_t)(__stop_fe8_data - __start_fe8_data);

    if (!Port_MapGbaMemory())
        return 1;
    if (!Port_HostInit(&cfg))
        return 1;

    // Game data comes from the player's ROM (nothing is embedded in the build).
    {
        const char* error = NULL;
        for (;;) {
            size_t size = 0;
            uint8_t* rom = Port_HostLoadRom(&size, error);
            if (!rom) {
                Port_HostShutdown();
                return 1;
            }
            PortRomStatus status = Port_LoadRom(rom, size);
            if (status == PORT_ROM_OK)
                Port_HostRomAccepted(rom, size);
            free(rom);
            if (status == PORT_ROM_OK)
                break;
            error = Port_RomStatusText(status);
            Port_Log("ROM rejected: %s", error);
        }
    }

    sInitialData = malloc(dataSize ? dataSize : 1);
    if (!sInitialData)
        Port_Fatal("out of memory");
    Port_RawCopy(sInitialData, __start_fe8_data, dataSize);

    // Erased flash reads as 0xFF; the host overwrites it with the save file.
    memset(gPortSram, 0xFF, sizeof(gPortSram));
    Port_HostLoadSave(gPortSram, sizeof(gPortSram));

    if (setjmp(sExitJump) == 0) {
        // src/crt0.s: `bl AgbMain; b crt0` — and SoftReset starts over too.
        setjmp(sResetJump);
        Port_FlushSave();
        Port_RawCopy(__start_fe8_data, sInitialData, dataSize);
        Port_RawZero(__start_fe8_bss, (size_t)(__stop_fe8_bss - __start_fe8_bss));
        Port_RawZero(__start_ewram_data, (size_t)(__stop_ewram_data - __start_ewram_data));
        Port_RawZero(__start_iwram_data, (size_t)(__stop_iwram_data - __start_iwram_data));
        Port_MemReset();
        Port_DmaReset();
        Port_ApuReset();
        Port_PpuInit();
        while (!sQuit)
            AgbMain();
    }

    Port_FlushSave();
    Port_HostShutdown();
    return 0;
}
