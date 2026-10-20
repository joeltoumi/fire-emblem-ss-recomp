#!/usr/bin/env python3
"""Find the addresses of the decomp's ROM symbols in another version's ROM.

    map_rom.py US_RELOCS.elf OTHER.gba ROM_SYMBOLS_US.txt OUT.txt [--report R.txt]

US_RELOCS.elf is the US build linked with --emit-relocs (see docs/PORT.md).
The other ROM shares most code and data with the US one, moved around. The
mapping is found in steps:

  1. every US function is searched for in the other ROM, byte for byte with
     the bytes its relocations cover masked out (literal pools, BL offsets);
  2. a matched function's relocations then give the other ROM's address of
     everything it references (the words of its literal pools, its BL
     targets);
  3. so do the relocations inside data that has been mapped (pointer
     tables, scripts...), repeated until nothing new is found;
  4. a symbol that nothing points to is searched for by its bytes near the
     address its neighbours moved to;
  5. one that is still not found but lies between two symbols that moved by
     the same amount moved by that amount.

Every symbol of ROM_SYMBOLS_US.txt must end up mapped; the ones that are
not are listed in the report (and the tool fails).
"""
import argparse
import bisect
import re
import struct
import sys
from collections import Counter, defaultdict

from elftools.elf.elffile import ELFFile

ROM = 0x08000000
R_ARM_ABS32 = 2
R_ARM_THM_CALL = 10


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def decode_bl(b, o, pc):
    hi = struct.unpack_from("<H", b, o)[0]
    lo = struct.unpack_from("<H", b, o + 2)[0]
    if (hi & 0xF800) != 0xF000 or (lo & 0xF800) not in (0xF800, 0xE800):
        return None
    off = ((hi & 0x7FF) << 12) | ((lo & 0x7FF) << 1)
    if off & 0x400000:
        off -= 0x800000
    return pc + 4 + off


class Us:
    def __init__(self, path):
        elf = ELFFile(open(path, "rb"))
        seg = elf.get_section_by_name("ROM")
        self.base = seg["sh_addr"]
        self.data = seg.data()
        self.relocs = {}  # address -> type
        for r in elf.get_section_by_name(".relROM").iter_relocations():
            self.relocs[r["r_offset"]] = r["r_info_type"]
        self.funcs = []  # (addr, size, name)
        self.funcs_by_addr = {}
        points = set()
        names = defaultdict(list)
        for s in elf.get_section_by_name(".symtab").iter_symbols():
            v = s["st_value"]
            if not (ROM <= v < ROM + len(self.data)) or s["st_info"]["type"] == "STT_SECTION":
                continue
            if s["st_info"]["type"] == "STT_FUNC":
                a = v & ~1
                if s["st_size"]:
                    self.funcs.append((a, s["st_size"], s.name))
                    self.funcs_by_addr[a] = s["st_size"]
                points.add(a)
            else:
                points.add(v)
            names[v & ~1].append(s.name)
        self.points = sorted(points)
        self.names = names
        self.mask = bytearray(len(self.data))  # 1 = differs per build
        for a, t in self.relocs.items():
            o = a - self.base
            n = 4
            for i in range(n):
                if 0 <= o + i < len(self.mask):
                    self.mask[o + i] = 1
        self.reloc_list = sorted(self.relocs)

    def byte(self, a):
        return self.data[a - self.base]

    def extent(self, a):
        """End of the object starting at a: the next symbol."""
        i = bisect.bisect_right(self.points, a)
        return self.points[i] if i < len(self.points) else self.base + len(self.data)

    def relocs_in(self, a, end):
        i = bisect.bisect_left(self.reloc_list, a)
        while i < len(self.reloc_list) and self.reloc_list[i] < end:
            yield self.reloc_list[i]
            i += 1

    def pattern(self, a, size):
        out = []
        o = a - self.base
        for i in range(size):
            if self.mask[o + i]:
                out.append(b".")
            else:
                out.append(re.escape(self.data[o + i : o + i + 1]))
        return re.compile(b"".join(out), re.DOTALL)


class Mapper:
    def __init__(self, us, other):
        self.us = us
        self.other = other
        self.map = {}  # US address -> other address (exact points)
        self.votes = defaultdict(Counter)

    def other_ok(self, a):
        return ROM <= a < ROM + len(self.other)

    def vote(self, us_addr, ot_addr):
        if self.other_ok(ot_addr):
            self.votes[us_addr][ot_addr] += 1

    def search(self, a, size, guess, window):
        pat = self.us.pattern(a, size)
        lo = max(0, guess - ROM - window)
        hi = min(len(self.other), guess - ROM + window + size)
        hits = [m.start() + ROM for m in pat.finditer(self.other, lo, hi)]
        return hits

    def delta_near(self, a):
        """Delta of the closest mapped point before a (0 if none)."""
        keys = self.sorted_keys
        i = bisect.bisect_right(keys, a) - 1
        if i < 0:
            return 0
        k = keys[i]
        return self.map[k] - k

    def resort(self):
        self.sorted_keys = sorted(self.map)

    def follow(self, us_start, us_end, ot_start):
        """Votes from the relocations of a mapped range."""
        for r in self.us.relocs_in(us_start, us_end):
            t = self.us.relocs[r]
            ot = ot_start + (r - us_start)
            if t == R_ARM_ABS32:
                tgt = u32(self.us.data, r - self.us.base)
                if ROM <= tgt < ROM + len(self.us.data) and self.other_ok(ot):
                    self.vote(tgt & ~1, u32(self.other, ot - ROM) & ~1)
            elif t == R_ARM_THM_CALL:
                tu = decode_bl(self.us.data, r - self.us.base, r)
                to = decode_bl(self.other, ot - ROM, ot) if self.other_ok(ot) else None
                if tu is not None and to is not None:
                    self.vote(tu, to)

    def run(self, wanted):
        us = self.us
        # 1. functions, in address order, tracking the running delta.
        delta = 0
        self.sorted_keys = []
        unmatched_funcs = []
        for a, size, name in sorted(us.funcs):
            if size < 8:
                continue
            hits = self.search(a, size, a + delta, 0x400)
            if len(hits) != 1:
                hits = self.search(a, size, a + delta, 0x40000)
            if len(hits) == 1:
                self.map[a] = hits[0]
                delta = hits[0] - a
            else:
                unmatched_funcs.append((a, name, len(hits)))
        for a, size, name in us.funcs:
            if a in self.map:
                self.follow(a, a + size, self.map[a])
        # Functions found more than once (small helpers, duplicated code):
        # the hit where the neighbours moved to, or where the calls go.
        self.resort()
        still = []
        for a, name, n in unmatched_funcs:
            size = us.funcs_by_addr[a]
            guess = a + self.delta_near(a)
            hits = self.search(a, size, guess, 0x40000) if n else []
            pick = None
            if guess in hits:
                pick = guess
            elif self.votes.get(a):
                oa, v = self.votes[a].most_common(1)[0]
                if not hits or oa in hits:
                    pick = oa
            if pick is not None:
                self.map[a] = pick
                if pick in hits:
                    self.follow(a, a + size, pick)
            else:
                still.append((a, name, n))
        self.unmatched_funcs = still
        # 2/3. data: settle votes, follow mapped data, repeat.
        while True:
            added = 0
            for ua, c in list(self.votes.items()):
                if ua in self.map:
                    continue
                oa, n = c.most_common(1)[0]
                if n * 2 <= sum(c.values()):
                    continue
                self.map[ua] = oa
                added += 1
                # a pointer into the middle of an object maps the object too
                start = self.containing(ua)
                end = us.extent(start)
                if start not in self.map:
                    self.map[start] = oa - (ua - start)
                self.follow(start, end, self.map[start])
            if not added:
                break
        self.resort()
        # 4. symbols nothing pointed to: by bytes, near the neighbours' delta.
        for ua in sorted(wanted):
            if ua in self.map:
                continue
            start = self.containing(ua)
            if start in self.map:
                self.map[ua] = self.map[start] + (ua - start)
                continue
            size = min(us.extent(ua) - ua, 0x200)
            if size < 4:
                continue
            guess = ua + self.delta_near(ua)
            hits = self.search(ua, size, guess, 0x10000)
            if not hits and size >= 16:
                hits = self.search(ua, size, guess, 0x400000)
            if len(hits) > 1:
                hits = sorted(hits, key=lambda h: abs(h - guess))[:1] if size >= 16 else []
            if len(hits) == 1:
                self.map[ua] = hits[0]
                self.follow(ua, us.extent(ua), hits[0])
        self.resort()
        # 5. what is left sits between two mapped symbols that moved by the
        # same amount (unused data, mostly): it moved by that amount too.
        for ua in sorted(wanted):
            if ua in self.map:
                continue
            i = bisect.bisect_right(self.sorted_keys, ua)
            if 0 < i < len(self.sorted_keys):
                a, b = self.sorted_keys[i - 1], self.sorted_keys[i]
                if self.map[a] - a == self.map[b] - b:
                    self.map[ua] = ua + self.map[a] - a
        self.resort()

    def containing(self, a):
        i = bisect.bisect_right(self.us.points, a) - 1
        return self.us.points[i] if i >= 0 else a


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("rom")
    ap.add_argument("symbols")
    ap.add_argument("out")
    ap.add_argument("--report")
    args = ap.parse_args()

    us = Us(args.elf)
    other = open(args.rom, "rb").read()
    syms = []
    for line in open(args.symbols):
        name, addr = line.split()
        syms.append((name, int(addr, 16)))
    m = Mapper(us, other)
    m.run({a for _, a in syms})

    missing = []
    with open(args.out, "w") as f:
        for name, a in syms:
            if a in m.map:
                f.write(f"{name} {m.map[a]:08x}\n")
            else:
                missing.append((name, a))
    rep = sys.stderr if not args.report else open(args.report, "w")
    print(f"functions: {len(us.funcs) - len(m.unmatched_funcs)}/{len(us.funcs)} matched", file=rep)
    for a, name, n in m.unmatched_funcs:
        guess = a + m.delta_near(a)
        print(f"  func {name} {a:08x} hits={n} near={guess:08x} size={us.funcs_by_addr[a]:x}", file=rep)
    print(f"symbols: {len(syms) - len(missing)}/{len(syms)} mapped", file=rep)
    for name, a in missing:
        print(f"  missing {name} {a:08x}", file=rep)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
