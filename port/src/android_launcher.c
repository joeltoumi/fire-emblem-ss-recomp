// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — Android launcher (libmain.so).
//
// The game stores pointers in 32-bit fields, as on the GBA, so its image
// must sit below 4 GiB, next to the GBA memory map (see docs/PORT.md). On
// desktop the executable is linked at that address; on Android the system
// loads libraries wherever it likes. So SDLActivity's libmain.so is only
// this launcher: it reserves address space at FE8_IMAGE_BASE and has the
// dynamic linker load the game, libgame.so, into it, then runs the game's
// SDL_main.
#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <stdint.h>
#include <sys/mman.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

// Room for libgame.so (its whole image, all segments).
#define GAME_RESERVED_SIZE (256u << 20)

#define LOG(...) __android_log_print(ANDROID_LOG_ERROR, "fe8", __VA_ARGS__)

__attribute__((visibility("default"))) int SDL_main(int argc, char** argv)
{
    void* want = (void*)(uintptr_t)FE8_IMAGE_BASE;
    void* base = mmap(want, GAME_RESERVED_SIZE, PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (base != want) {
        LOG("cannot reserve the game's address range at %p", want);
        return 1;
    }

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
