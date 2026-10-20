#!/bin/sh
# Regenerates port/data/{rom,ram}_symbols_eu.txt from the EU ROM.
#
#   tools/port/map_eu.sh RELOCS.elf EU.gba
#
# RELOCS.elf: the US GBA build linked with --emit-relocs:
#   arm-none-eabi-ld -T ldscript.txt --emit-relocs @objects.lst \
#       -R data/banim/data_banim.o.sym.o -L tools/agbcc/lib -o RELOCS.elf -lc -lgcc
# Addresses found by hand are in port/data/eu_fixes/*.txt.
set -e
cd "$(dirname "$0")/../.."
fixes=""
for f in port/data/eu_fixes/*.txt; do fixes="$fixes --fixes $f"; done
python3 tools/port/map_rom.py "$1" "$2" --guess $fixes \
    --map port/data/rom_symbols_us.txt port/data/rom_symbols_eu.txt \
    --map port/data/ram_symbols_us.txt port/data/ram_symbols_eu.txt \
    --report port/data/map_eu_report.txt
grep -E "symbols_eu.txt:" port/data/map_eu_report.txt
