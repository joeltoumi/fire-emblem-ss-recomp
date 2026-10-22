# How the port works

The port compiles the decompiled game code (`src/`) natively and runs it on a
small model of the GBA hardware (`port/`). Game data is never converted or
embedded: it is read in place from the player's ROM.

## Memory

- The GBA memory regions (EWRAM, IWRAM, I/O, palette, VRAM, OAM, ROM, SRAM)
  are mapped at their GBA addresses (`port/src/gba_mem.c`), so a GBA pointer
  is also a valid native pointer.
- The game image is linked below 4 GiB (`FE8_IMAGE_BASE`, `0x10000000`): the
  game keeps pointers in 32-bit fields. On Android, `libmain.so` reserves a free
  range below 4 GiB (ART keeps its heap there too, so the address varies)
  and loads `libgame.so` into it with `android_dlopen_ext`.
- The ROM is loaded at `0x08000000`. Every data symbol the code reads from it
  is an absolute symbol at its ROM address, listed per region in
  `port/data/rom_symbols_{us,eu}.txt` and turned into an assembly file of
  absolute symbol definitions by
  `tools/port/gen_abs_symbols.py`. RAM symbols whose GBA address matters are
  in `port/data/ram_symbols_*.txt`.

## ROM data with pointers

Structs read from the ROM keep their GBA layout: their pointer fields are
32-bit (`GBA_PTR(type)`), read with `GBA_GET(type, x)`. Function pointers in
ROM data (event ASMC routines, proc callbacks, anim script calls) go through
`Port_GbaFunc`, a table from ROM address to native function generated from
`port/data/rom_funcs_*.txt` (`tools/port/gen_gba_funcs.py`).

## Regions

The code targets one region per build (`-DFE8_REGION=us|eu`, `REGION_EU`
branches in the code). The decompilation is of the US version; the EU
addresses are found by `tools/port/map_eu.sh`, which matches the US build
against the EU ROM. The US build used for matching has no bytes for `incbin`
data, so some data symbols need their EU address set by hand: those are in
`port/data/eu_fixes/*.txt`, each with how it was found. Message ids written
in the code are US ids; `MSG_ID()` (`include/port/msg_id.h`) maps them to the
EU numbering.

## Frames, video and sound

`VBlankIntrWait()` runs one frame of hardware (`port/src/frame.c`): the VBlank
interrupt, the 160 visible lines through the software PPU (VirtuaPPU, with
HBlank DMA and interrupts between lines), the sound mixer, and presenting the
frame. The host (`host_sdl.c` or `host_headless.c`) shows it and reads input.

## Save states

A state is the GBA RAM, VRAM, palette, OAM and I/O, every game global (the
`fe8_data`, `fe8_bss`, `ewram_data` and `iwram_data` sections) and the port's
own hardware state (`fe8_state`). It is taken and restored only at the end of
an iteration of `AgbMain`'s loop, where the C stack is always the same.

## Second screen

On devices with two displays (AYN Thor) the lower one shows, as the player
chooses in the options menu, the whole battle map, the unit under the cursor
(or the battle forecast) or the army list; on PC it opens in a second window.
`port/game/screen2_game.c` reads the game state into the plain structure of
`include/port/screen2.h` (the map is drawn from the chapter's own tileset and
palettes, the units with their standing sprites), and
`port/src/second_screen.c` draws it into a 256-pixel-wide canvas and turns
touches into cursor moves and GBA button presses. The cursor is only moved
between two frames, while the player phase is waiting for input, the same way
the game moves it with L. On Android, `Fe8Activity` shows the canvas on the
secondary display as a `Presentation`; its JNI functions are registered by
`host_sdl.c` because `libgame.so` is loaded by the launcher.

`fe8_headless --screen2 MODE` dumps the second screen next to each frame and
`--touch2 FRAME:X:Y` taps it, so this can be tested without a device.

## Widescreen

With the widescreen option the picture is wider than 240 pixels (up to 320)
on the battle map. The game still runs at 240x160: its camera, its logic and
its tilemaps don't change. The terrain (BG3) and movement range (BG2)
tilemaps are 256-pixel rings that only hold what the GBA screen needs, so
before each frame `port/game/widescreen_game.c` rebuilds the whole map the
way `DisplayBmTile` and `DisplayMovementViewTile` draw it and hands it to the
PPU, which uses it for the columns outside the GBA screen
(`virtuappu_mode1_ws_world`). It is only used when it matches what the game's
own tilemap shows on the GBA screen; otherwise (battle scenes, the status
screen, cutscenes) the frame is drawn at 240 pixels in the middle. Unit
sprites are drawn further out (`PutUnitSpritesOam`), their OAM entries are
tagged as map objects, and weather particles, which wrap every 256 pixels,
are repeated across the frame; every other sprite and the window/menu layers
stay on the GBA screen. `port/src/widescreen.c` places the GBA screen inside
the frame (centered, sliding off-center near the map edges, with black beyond
the map on narrow maps).

`fe8_headless --width W` renders at that width (284 = 16:9).

## Differences from the GBA that need code changes

Most fixes in `src/` are under `#ifdef PLATFORM_PC`:

- reads through NULL, which read the BIOS on the GBA and crash natively;
- structs that are larger natively because of 64-bit pointers, where the
  code assumes the GBA size or offsets;
- division by zero (0 on the GBA, a trap on x86);
- buffers that the code overruns into the next variable, as the GBA layout
  allows (`EWRAM_OVERLAY` variables stay contiguous).

## Testing against an emulator

`fe8_headless` takes scripted input (`--press FRAME:KEYS:LEN`) and writes
frames (`--dump-every`) and raw video memory (`--dump-mem FRAME:FILE`), so a
run can be compared with the same input in an emulator such as mGBA.

To test later chapters without playing up to them, set `FE8_WARP` to a chapter
index (see `include/constants/chapters.h`): a new game then starts on that
chapter, skipping the world map. Units from earlier chapters are missing, so
events that expect them may not behave as in a real playthrough.
