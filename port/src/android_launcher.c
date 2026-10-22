// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — Android launcher (libmain.so).
//
// The game stores pointers in 32-bit fields, as on the GBA, so its image
// must sit below 4 GiB, next to the GBA memory map (see docs/PORT.md). On
// desktop the executable is linked at that address; on Android the system
// loads libraries wherever it likes. So SDLActivity's libmain.so is only
// this launcher: it reserves address space below 4 GiB and has the dynamic
// linker load the game, libgame.so, into it, then runs the game's SDL_main.
// The address itself doesn't matter (libgame.so is position independent),
// and ART keeps its heap in the low 4 GiB too, so it tries FE8_IMAGE_BASE
// first and then looks for a free range.
#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <stdint.h>
#include <sys/mman.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

// Room for libgame.so (its whole image, all segments: about 20 MiB).
#define GAME_RESERVED_SIZE (64u << 20)
// Where to look: above the GBA memory map, below 4 GiB.
#define SEARCH_START 0x10000000u
#define SEARCH_END 0xFC000000u
#define SEARCH_STEP (16u << 20)

#define LOG(...) __android_log_print(ANDROID_LOG_ERROR, "fe8", __VA_ARGS__)

static void* Reserve(uintptr_t address)
{
    void* want = (void*)address;
    void* got = mmap(want, GAME_RESERVED_SIZE, PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (got == want)
        return got;
    // Taken (older kernels ignore MAP_FIXED_NOREPLACE and map elsewhere).
    if (got != MAP_FAILED)
        munmap(got, GAME_RESERVED_SIZE);
    return NULL;
}

__attribute__((visibility("default"))) int SDL_main(int argc, char** argv)
{
    void* base = Reserve(FE8_IMAGE_BASE);
    for (uintptr_t a = SEARCH_START; !base && a + GAME_RESERVED_SIZE <= SEARCH_END; a += SEARCH_STEP)
        base = Reserve(a);
    if (!base) {
        LOG("cannot reserve an address range below 4 GiB for the game");
        return 1;
    }
    __android_log_print(ANDROID_LOG_INFO, "fe8", "game image at %p", base);

    android_dlextinfo info = {
        .flags = ANDROID_DLEXT_RESERVED_ADDRESS,
        .reserved_addr = base,
        .reserved_size = GAME_RESERVED_SIZE,
    };
    void* game = android_dlopen_ext("libgame.so", RTLD_NOW | RTLD_LOCAL, &info);
    if (!game) {
        LOG("cannot load libgame.so: %s", dlerror());
        return 1;
    }
    int (*gameMain)(int, char**) = (int (*)(int, char**))dlsym(game, "SDL_main");
    if (!gameMain) {
        LOG("libgame.so has no SDL_main");
        return 1;
    }
    return gameMain(argc, argv);
}
