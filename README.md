# Fire Emblem: The Sacred Stones — native PC/Android port

**fire-emblem-ss-recomp** builds Fire Emblem: The Sacred Stones as a native
program for Linux and Android, from the
[fireemblem8u](https://github.com/FireEmblemUniverse/fireemblem8u) decompilation.
It is not an emulator: the game's C code is compiled for your machine, and its
data (graphics, text, maps, music) is read from your own ROM at startup.

**You need your own legally obtained ROM.** None of the game's data is in this
repository or in the builds.

| ROM | Status |
| --- | --- |
| Europe (En, Fr, De, Es, It) | Prologue and first battle play through (tested in English) |
| USA | Builds; not tested in play yet |

What the port adds: an options menu (Esc / F1, Select+Start, or the menu button
on touch screens) with 4 quick-save slots with thumbnails (F5 / F8), remappable
keyboard and gamepad controls, on-screen touch controls, fast forward, and LCD/CRT filters.

## Playing

- **Android:** install the APK for your ROM's region (eu or us, from the
  [build workflow](../../actions/workflows/build.yml) artifacts) and
  pick your ROM the first time.
- **PC:** build it (below) and run `fe8`. It asks for the ROM the first time
  and remembers it.

## Building

```bash
cmake -B build -G Ninja -DFE8_REGION=eu        # or us
cmake --build build
./build/fe8
```

Needs CMake 3.22+, Ninja, clang (or GCC) and lld. SDL3 and Dear ImGui are
downloaded when they are not installed. `-DFE8_HOST=headless` builds
`fe8_headless`, a windowless version for tests (`--press`, `--dump-every`,
`--dump-mem`, `--state-save`/`--state-load`).

Android: `cd android && ./gradlew assembleEuRelease` (or `assembleUsRelease`).

How the port works is in [docs/PORT.md](docs/PORT.md).

---

# The decompilation


[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)

[FE Decomp Portal](https://laqieer.github.io/fe-decomp-portal/)

This is a disassembly of Fire Emblem: The Sacred Stones (U)[!]

It builds the following ROM:
* fireemblem8.gba `sha1: c25b145e37456171ada4b0d440bf88a19f4d509f`

### Quick Start

If you just want to get the repo building quickly (Ubuntu/WSL, Arch Linux, or macOS/Homebrew), drop your legally obtained ROM at the repo root as `baserom.gba` and run:

```
./scripts/quickstart.sh [--rom /path/to/baserom.gba] [--refresh-agbcc]
```

See [`docs/quickstart.md`](docs/quickstart.md) for full details, flags, and troubleshooting tips.

### Building

To build the ROM:
```bash
make fireemblem8.gba -j$(nproc)
```
To clean all build artifacts:
```bash
make clean
```
To clean all build artifacts **except** the extremely slow battle animation compression outputs:
```bash
make clean_fast
```

### Setting up the repository manually

1. You must have a copy of the Fire Emblem: The Sacred Stones ROM named `baserom.gba` in the repository directory.
2. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) or [GNU Arm Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm).
```
# for Ubuntu/WSL users
apt install binutils-arm-none-eabi
```
3. Install [agbcc](https://github.com/pret/agbcc) to this project.
```
cd /path/to/agbcc
./build.sh
./install.sh /path/to/fireemblem8u
```
4. Build tools.
```
cd /path/to/fireemblem8u
./build_tools.sh
```
5. Build the project.
```
make
```
6. You will see this for success.
```
fireemblem8.gba: OK
```

Q: `fatal error: png.h: No such file or directory`

A: Install [libpng](http://www.libpng.org/pub/png/libpng.html) to build `tools/gbagfx`.

Q: `make: *** No rule to make target 'baserom.gba', needed by 'xxx'.  Stop.`

A: You must place a copy of the Fire Emblem: The Sacred Stones ROM named `baserom.gba` in the repository directory.

Q: `unrecognized option '--add-symbol'`

A: Update your devkitPro or embedded toolchain. Read [this](https://github.com/bminor/binutils-gdb/blob/3451a2d7a3501e9c3fc344cbc4950c495f30c16d/binutils/ChangeLog-2015#L120) for more info.

Q: `.dep/src/xxx.d:2: *** missing separator.  Stop.`

A: `rm -rf .dep` or disable [VSCode Extension: Makefile Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.makefile-tools) if installed.

Check [INSTALL.md](https://github.com/pret/pokeruby/blob/master/INSTALL.md) and [INSTALL.md](https://github.com/pret/pokeemerald/blob/master/INSTALL.md) if you have trouble in setting up.

Check [remove_tools](https://github.com/laqieer/fireemblem8u/tree/remove_tools) branch if you don't want to build agbcc and other tools by yourself. It uses docker to make setting up easier. Follow its [README.md](https://github.com/laqieer/fireemblem8u/blob/remove_tools/README.md) instead.
