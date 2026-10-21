# fire-emblem-ss-recomp

Native port of **Fire Emblem: The Sacred Stones** (GBA) to Android and PC, built
from the [fireemblem8u](https://github.com/FireEmblemUniverse/fireemblem8u)
decompilation. It is not an emulator: the game code is compiled as C for the
device, with an SDL3 platform layer (the same approach as
[mzm-android](https://github.com/joeltoumi/mzm-android) and
[mf-android](https://github.com/joeltoumi/mf-android)).

The repository **contains no game data**. On launch, the app asks for your own
copy of the ROM and reads the graphics, maps, text and music from it.

## Status

- European (EU) and American (US) versions. Each build accepts only its own ROM:

  | Region | File | SHA-1 |
  |---|---|---|
  | EU | Fire Emblem - The Sacred Stones (Europe) (En,Fr,De,Es,It) | `51b166dcca231369d7f98000766746dd4b98839c` |
  | US | Fire Emblem - The Sacred Stones (USA) | `c25b145e37456171ada4b0d440bf88a19f4d509f` |

- Tested on PC with the EU ROM:
  - the whole prologue, compared frame by frame with mGBA (same dialogue,
    combat, damage and level-ups);
  - every chapter of both routes, the Tower of Valni and the Lagdou Ruins,
    started directly and played with random input to find crashes;
  - menus, saving, the combat animations and save states.
- The US build compiles, but has not been tested with a US ROM yet.
- Still to do: a full playthrough by hand, testing on Android devices, and
  widescreen.

## What the port adds

- **Options menu** (Esc / F1, Select+Start on a gamepad, or the menu button on
  touch screens).
- **Save states**: 4 quick slots with thumbnails (F5 saves, F8 loads).
- **Controls**: remappable keyboard and gamepad, on-screen touch controls on
  Android.
- **Fast forward** (Tab, or the right stick on a gamepad).
- **Screen filters**: LCD and CRT.
- Fullscreen with F11.

Default keyboard controls:

| GBA | Key |
|---|---|
| A / B | X / Z |
| L / R | A / S |
| Start / Select | Enter / Backspace |
| D-pad | Arrow keys |

## Downloading the APK

GitHub Actions builds one APK per region on every change to `main`. Download
it from the **Actions** tab: open the latest
[`build`](../../actions/workflows/build.yml) run and grab the
`fe8-android-apk` artifact. Install the one for your region and pick your ROM
when you open it.

## Building

### Android

You need JDK 17 and the Android SDK with NDK 27.2.12479018:

```
cd android
./gradlew assembleEuRelease   # or assembleUsRelease
```

### PC (Linux)

You need CMake 3.22+, Ninja, clang (or GCC) with lld, and Python 3. SDL3 and
Dear ImGui are downloaded automatically if they are not installed.

```
cmake -B build -G Ninja -DFE8_REGION=eu    # or us
ninja -C build
./build/fe8 [path/to/rom.gba]
```

Without a path, the program looks for `fe8_eu.gba` / `fe8_us.gba` next to the
executable or opens a file picker, and remembers the ROM afterwards.

### Testing

`-DFE8_HOST=headless` builds `fe8_headless`, a windowless version that takes
scripted input and dumps screenshots, memory and save states. The `FE8_WARP`
variable starts a new game on any chapter. Both are described in
[docs/PORT.md](docs/PORT.md), along with how the port works.

## GBA ROM (decompilation)

The tree is still the fireemblem8u decompilation, and the port's changes to
the game code are under `#ifdef PLATFORM_PC`. The instructions for building
the original GBA ROM are in [docs/UPSTREAM_README.md](docs/UPSTREAM_README.md).

## Credits

- Decompilation: [FireEmblemUniverse/fireemblem8u](https://github.com/FireEmblemUniverse/fireemblem8u)
  and its contributors ([FE Decomp Portal](https://laqieer.github.io/fe-decomp-portal/)).
- Fire Emblem is a trademark of Nintendo and Intelligent Systems. This project
  is not affiliated with them and does not distribute any game data.
