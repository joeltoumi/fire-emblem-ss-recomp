#!/usr/bin/env python3
"""Place the RAM variables that only the GBA linker script defines (fe8 port).

ldscript.txt and sym_iwram.txt give many IWRAM/EWRAM variables an address
without any C definition. The port maps EWRAM and IWRAM at their GBA
addresses, so most of these simply stay where they are on the GBA: an
absolute symbol at that address (port/data/ram_symbols_us.txt). The game
relies on that layout in places: it clears or copies past the end of one
variable into the next ones, or uses an array of unknown size.

That only works when the variable's native size fits in its GBA slot (the
gap to the next GBA symbol). A variable whose native type is larger (it
holds pointers, which are 8 bytes natively) is defined natively instead:

  * declared in a header with a complete type: `__typeof__(name) name;`;
  * else, declared with a complete type by an `extern` inside a source file:
    that declaration, as a definition.

A variable with no usable declaration (an array of unknown size, a type
local to a source file...) is always placed at its GBA address.

port/game/ram_native.c defines a few by hand; they are skipped here.

Writes port/game/ram_symbols.c and port/data/ram_symbols_us.txt. The native
sizes are measured by compiling and running a probe with CC, so run it on
a 64-bit (LP64) host: the sizes are the same on every 64-bit target.

Usage: gen_ram_symbols.py fireemblem8.elf CC
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# Linker bookkeeping, or provided by the port itself.
SKIP = {
    "__ewram_start", "__iwram_start", "__iwram_top", "__sp_irq", "__sp_usr",
    "__irq_flags", "end", "errno", "IntrMain_Buffer",
}


def nm(*args):
    out = subprocess.run(["arm-none-eabi-nm", *args], check=True, capture_output=True, text=True).stdout
    return [line.split() for line in out.splitlines()]


def main():
    elf, cc = sys.argv[1], sys.argv[2]
    src_defs = set()
    for obj in list(ROOT.glob("src/*.o")) + list(ROOT.glob("asm/*.o")):
        for parts in nm("-g", "--defined-only", str(obj)):
            if len(parts) == 3:
                src_defs.add(parts[2])
    syms = {}
    all_addrs = sorted({int(p[0], 16) for p in nm("--defined-only", elf) if int(p[0], 16) >> 24 in (2, 3)})
    for parts in nm("-g", "--defined-only", elf):
        addr, name = int(parts[0], 16), parts[-1]
        if addr >> 24 in (2, 3) and name not in src_defs and name not in SKIP:
            syms[name] = addr

    manual = set(re.findall(r"^[^/#\s][^;(]*?\b(\w+)\s*(?:\[[^\]]*\]\s*)*(?:=[^;]*)?;",
                            (ROOT / "port/game/ram_native.c").read_text(), re.M))
    for n in manual:
        syms.pop(n, None)

    headers = ['#include "global.h"'] + [f'#include "{h.name}"' for h in sorted(ROOT.glob("include/*.h"))]
    native = sorted(syms)
    while True:
        body = "\n".join(headers) + "\n\n" + "".join(f"__typeof__({n}) {n};\n" for n in native)
        with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as f:
            f.write(body)
        res = subprocess.run([cc, "-fsyntax-only", "-std=gnu11", "-Wno-everything", "-ferror-limit=0",
                              "-DPLATFORM_PC", "-DNONMATCHING=1", f"-I{ROOT/'include'}", "-iquote", str(ROOT),
                              f"-I{ROOT/'port/generated'}", "-include", "port/native.h", f.name],
                             capture_output=True, text=True)
        bad = set()
        lines = body.splitlines()
        for m in re.finditer(r":(\d+):\d+: error:", res.stderr):
            line = lines[int(m.group(1)) - 1]
            m2 = re.match(r"__typeof__\((\w+)\)", line)
            if m2:
                bad.add(m2.group(1))
        if not bad:
            if res.returncode != 0:
                sys.exit(res.stderr)
            break
        native = [n for n in native if n not in bad]

    # Declarations in source files: `extern TYPE name...;`.
    local = {}
    for src in sorted(ROOT.glob("src/*.c")):
        for m in re.finditer(r"^\s*extern\s+([^;()]*?\b(\w+)\s*((?:\[[^\]]+\]\s*)*));", src.read_text(errors="replace"), re.M):
            decl, name, dims = m.group(1), m.group(2), m.group(3)
            if name in syms and name not in native and name not in local:
                local[name] = decl
        # function pointers: `extern RET (*name)(ARGS);`
        for m in re.finditer(r"^\s*extern\s+([^;]*?\(\s*\*\s*(\w+)\s*\)\s*\([^;]*\))\s*;", src.read_text(errors="replace"), re.M):
            decl, name = m.group(1), m.group(2)
            if name in syms and name not in native and name not in local:
                local[name] = decl
    while True:
        body = "\n".join(headers) + "\n\n" + "".join(f"{d};\n" for d in local.values())
        with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as f:
            f.write(body)
        res = subprocess.run([cc, "-fsyntax-only", "-std=gnu11", "-Wno-everything", "-ferror-limit=0",
                              "-DPLATFORM_PC", "-DNONMATCHING=1", f"-I{ROOT/'include'}", "-iquote", str(ROOT),
                              f"-I{ROOT/'port/generated'}", "-include", "port/native.h", f.name],
                             capture_output=True, text=True)
        lines = body.splitlines()
        bad = set()
        for m in re.finditer(r":(\d+):\d+: error:", res.stderr):
            line = lines[int(m.group(1)) - 1]
            for n, d in local.items():
                if line == d + ";":
                    bad.add(n)
        if not bad:
            break
        for n in bad:
            del local[n]

    # Sizes on the GBA (32-bit ARM) and natively (any 64-bit target), from
    # probe variables `char __sz_X[sizeof(X)]` compiled for each. An array of
    # unknown size is measured by its element (`__el_X`).
    decls = "".join(f"extern {d};\n" for d in local.values())
    names = list(native) + list(local)
    elem = set()
    while True:
        probe = "\n".join(headers) + "\n\n" + decls + "".join(
            f"char __el_{n}[sizeof(({n})[0])];\n" if n in elem else f"char __sz_{n}[sizeof({n})];\n" for n in names)
        sizes = {}
        lines = probe.splitlines()
        redo = set()
        for target, extra in (("armv4t-none-eabi", ["-isystem", "/usr/lib/arm-none-eabi/include"]),
                              ("x86_64-linux-gnu", [])):
            res = subprocess.run([cc, f"--target={target}", *extra, "-S", "-emit-llvm", "-o", "-", "-x", "c", "-",
                                  "-std=gnu11", "-w", "-ferror-limit=0", "-DPLATFORM_PC", "-DNONMATCHING=1",
                                  f"-I{ROOT/'include'}", "-iquote", str(ROOT), f"-I{ROOT/'port/generated'}",
                                  "-include", "port/native.h", "-funsigned-char"],
                                 input=probe, capture_output=True, text=True)
            for m in re.finditer(r"<stdin>:(\d+):\d+: error:", res.stderr):
                m2 = re.match(r"char __sz_(\w+)\[", lines[int(m.group(1)) - 1])
                if m2:
                    redo.add(m2.group(1))
            if redo:
                break
            if res.returncode != 0:
                sys.exit(res.stderr)
            for m in re.finditer(r"^@__(sz|el)_(\w+) = .*?\[(\d+) x i8\]", res.stdout, re.M):
                sizes.setdefault(m.group(2), []).append(int(m.group(3)))
        if not redo:
            break
        elem |= redo

    def gap(a):
        return next((x for x in all_addrs if x > a), a + 0x10) - a

    def fits(n):
        if n not in sizes:
            return True  # no declaration: only ever used through its address
        gba, host = sizes[n]
        return host == gba if n in elem else host <= gap(syms[n])

    placed = sorted((n for n in syms if fits(n)), key=lambda n: syms[n])
    native = [n for n in native if n not in placed]
    local = {n: d for n, d in local.items() if n not in placed}

    with (ROOT / "port" / "data" / "ram_symbols_us.txt").open("w") as f:
        for n in placed:
            f.write(f"{n} {syms[n]:08x}\n")

    def note(n):
        gba, host = sizes[n]
        what = "element " if n in elem else ""
        return f"GBA 0x{syms[n]:08x}, {what}0x{gba:x} bytes on the GBA, 0x{host:x} natively"

    out = ROOT / "port" / "game" / "ram_symbols.c"
    with out.open("w") as f:
        f.write("// Generated by tools/port/gen_ram_symbols.py: do not edit.\n//\n")
        f.write("// RAM variables that only the GBA linker script places and that are\n")
        f.write("// larger natively than their GBA slot: native variables. The others stay\n")
        f.write("// at their GBA address (port/data/ram_symbols_*.txt).\n\n")
        f.write("\n".join(headers) + "\n\n")
        for n in native:
            if n in elem:
                # An array of unknown size: as many elements as its GBA slot holds.
                count = gap(syms[n]) // sizes[n][0]
                f.write(f"__typeof__({n}[0]) {n}[{count}]; // {note(n)}\n")
            else:
                f.write(f"__typeof__({n}) {n}; // {note(n)}\n")
        f.write("\n// Declared in source files only.\n")
        for n, d in local.items():
            f.write(f"{d}; // {note(n)}\n")
    print(f"{len(placed)} at their GBA address, {len(native) + len(local)} native", file=sys.stderr)


if __name__ == "__main__":
    main()
