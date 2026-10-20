#!/usr/bin/env python3
"""List the ROM data defined outside the C sources (fe8 native port).

Everything the decomp builds from assembly or binary files (data/*.s, the
graphics, songs and texts) is not compiled into the native build: the game
reads it in place from the player's ROM, which the port maps at its GBA
address (0x08000000). This tool lists those symbols with their address in a
built ROM, so tools/port/gen_rom_symbols.py can turn them into absolute
symbols.

Run after a GBA build (a ROM built from a zero-filled baserom.gba has the
same layout as the real one):

    tools/port/dump_rom_symbols.py fireemblem8.elf > port/data/rom_symbols_us.txt
"""
import subprocess
import sys
from pathlib import Path

NM = "arm-none-eabi-nm"


def nm(path, *args):
    out = subprocess.run([NM, *args, str(path)], check=True, capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            yield parts
        elif len(parts) == 4:  # with --print-size
            yield parts


def main():
    elf = Path(sys.argv[1])
    root = elf.parent
    addrs = {}
    sizes = {}
    for parts in nm(elf, "--defined-only", "-S"):
        if len(parts) == 4:
            addr, size, kind, name = parts
            sizes[name] = int(size, 16)
        else:
            addr, kind, name = parts
        addrs.setdefault(name, int(addr, 16))

    objs = []
    for top in ("data", "graphics", "sound"):
        objs += sorted((root / top).rglob("*.o"))
    # The texts: src/msg_data.c is generated from texts/ (the English script
    # the decomp ships); the port uses the ROM's own text instead.
    objs.append(root / "src" / "msg_data.o")
    seen = set()
    rows = []
    for obj in objs:
        for parts in nm(obj, "-g", "--defined-only"):
            _, kind, name = parts[-3:]
            # Linker bookkeeping labels (__data_start...) clash with the host's.
            if kind in "aA" or name in seen or name.startswith("__"):
                continue
            addr = addrs.get(name)
            if addr is None or not 0x08000000 <= addr < 0x0A000000:
                continue
            seen.add(name)
            rows.append((addr, name))
    rows.sort()
    for addr, name in rows:
        print(f"{name} {addr:08x}")
    print(f"{len(rows)} symbols", file=sys.stderr)


if __name__ == "__main__":
    main()
