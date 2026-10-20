#!/usr/bin/env python3
"""List the game functions that ROM data points to (fe8 port).

Some data the port reads from the ROM holds GBA addresses of game functions:
the ASMC commands of event scripts, the conditions of event lists... The
port translates those addresses to its native functions through a table
(port/game/gba_funcs.c, generated at build time from this list).

Writes "name address" lines (US addresses, without the Thumb bit) for every
function a 32-bit pointer in the ROM data listed in port/data/rom_symbols_us.txt
points to.

Usage: gen_rom_funcs.py RELOCS.elf > port/data/rom_funcs_us.txt
(RELOCS.elf: the US build linked with --emit-relocs, see tools/port/map_eu.sh.)
"""
import bisect
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

ROOT = Path(__file__).resolve().parents[2]
R_ARM_ABS32 = 2


def main():
    elf = ELFFile(open(sys.argv[1], "rb"))
    symtab = elf.get_section_by_name(".symtab")
    syms = list(symtab.iter_symbols())
    funcs = {}
    for s in syms:
        if s["st_info"]["type"] == "STT_FUNC" and 0x08000000 <= s["st_value"] < 0x0A000000:
            funcs.setdefault(s["st_value"] & ~1, s.name)
    # The ROM data the port reads, by address. Each extends up to the next
    # symbol of any kind (the native data in between is not read from the ROM).
    data = {int(a, 16) for _, a in (l.split() for l in open(ROOT / "port/data/rom_symbols_us.txt"))}
    bounds = sorted({s["st_value"] & ~1 for s in syms if s.name and not s.name.startswith("$")
                     and 0x08000000 <= s["st_value"] < 0x0A000000})
    code = sorted((s["st_value"] & ~1, s["st_size"]) for s in syms if s["st_info"]["type"] == "STT_FUNC")
    code_starts = [a for a, _ in code]

    def in_code(a):
        i = bisect.bisect_right(code_starts, a) - 1
        return i >= 0 and a < code[i][0] + code[i][1]

    def in_data(a):
        i = bisect.bisect_right(bounds, a) - 1
        return i >= 0 and bounds[i] in data and not in_code(a)

    out = {}
    for sec in elf.iter_sections():
        if not isinstance(sec, RelocationSection) or sec.name != ".relROM":
            continue
        for r in sec.iter_relocations():
            if r["r_info_type"] != R_ARM_ABS32:
                continue
            target = syms[r["r_info_sym"]]
            if target["st_info"]["type"] != "STT_FUNC":
                continue
            if in_data(r["r_offset"]):
                a = target["st_value"] & ~1
                out[a] = funcs[a]
    for a in sorted(out):
        print(f"{out[a]} {a:08x}")
    print(f"{len(out)} functions", file=sys.stderr)


if __name__ == "__main__":
    main()
