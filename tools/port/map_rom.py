#!/usr/bin/env python3
"""Find the addresses of the decomp's ROM symbols in another version's ROM.

    map_rom.py US_RELOCS.elf OTHER.gba --map SYMBOLS_US.txt OUT.txt [--map ...] [--report R.txt]

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

RAM symbols are mapped from the literal pools of the matched code (the same
word in both versions), or by their neighbours.

Every symbol of the lists must end up mapped; the ones that are not are
listed in the report (and the tool fails).
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


def is_ram(a):
    return 0x02000000 <= a < 0x02040000 or 0x03000000 <= a < 0x03008000


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
        ram_points = set()
        for s in elf.get_section_by_name(".symtab").iter_symbols():
            v = s["st_value"]
            if is_ram(v) and s["st_info"]["type"] != "STT_SECTION":
                ram_points.add(v)
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
        self.ram_points = sorted(ram_points)
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
        self.ram_votes = defaultdict(Counter)  # US RAM symbol -> Counter(delta)

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
                elif is_ram(tgt) and self.other_ok(ot):
                    v = u32(self.other, ot - ROM)
                    if is_ram(v):
                        pts = self.us.ram_points
                        sym = pts[max(bisect.bisect_right(pts, tgt) - 1, 0)]
                        self.ram_votes[sym][v - tgt] += 1
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
        self.fuzzy = []  # mapped by the calls to them, but their code differs
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
                    self.fuzzy.append(a)
            else:
                still.append((a, name, n))
        self.unmatched_funcs = still
        # 2/3. data: settle votes, follow mapped data, repeat.
        self.settle()
        # Functions whose code differs: their literal pools, lined up.
        self.resort()
        self.weak_votes = defaultdict(Counter)
        for a in self.fuzzy:
            self.align_literals(a)
        for ua, c in self.weak_votes.items():
            if ua not in self.map and not self.votes.get(ua):
                self.votes[ua] = c
        self.settle()
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

    def settle(self):
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
                end = self.us.extent(start)
                if start not in self.map:
                    self.map[start] = oa - (ua - start)
                self.follow(start, end, self.map[start])
            if not added:
                break

    def align_literals(self, a):
        """Votes from a function whose code differs in the other ROM.

        Its literal pools hold mostly the same pointers in the same order:
        the ones already mapped line the two lists up, and an unmapped US
        pointer between two lined-up ones takes the other ROM's word at the
        same place."""
        us = self.us
        size = us.funcs_by_addr[a]
        ot = self.map[a]
        i = bisect.bisect_right(self.sorted_keys, ot - (self.map[a] - a))
        # the other version's function ends where the next mapped one starts
        nxt = [self.map[k] for k in self.sorted_keys[i:i + 4] if self.map[k] > ot]
        ot_end = min(nxt + [ot + size * 2])
        ulits = [u32(us.data, r - us.base) for r in us.relocs_in(a, a + size)
                 if us.relocs[r] == R_ARM_ABS32]
        olits = [u32(self.other, o - ROM) for o in range(ot, ot_end, 4)
                 if ROM <= u32(self.other, o - ROM) < ROM + len(self.other)]
        ulits = [x for x in ulits if ROM <= x < ROM + len(us.data)]
        trans = [self.map.get(x & ~1) for x in ulits]
        # anchors: mapped US literals found in order in the other list
        j = 0
        anchors = [(-1, -1)]
        for k, t in enumerate(trans):
            if t is None:
                continue
            for jj in range(j, len(olits)):
                if olits[jj] & ~1 == t:
                    anchors.append((k, jj))
                    j = jj + 1
                    break
        anchors.append((len(ulits), len(olits)))
        for (k0, j0), (k1, j1) in zip(anchors, anchors[1:]):
            if k1 - k0 == j1 - j0:
                for d in range(1, k1 - k0):
                    if trans[k0 + d] is None:
                        self.weak_votes[ulits[k0 + d] & ~1][olits[j0 + d] & ~1] += 1

    def map_ram(self, wanted):
        """RAM addresses: from the literal pools of the matched code."""
        out = {}
        for ua in wanted:
            c = self.ram_votes.get(ua)
            if c:
                d, n = c.most_common(1)[0]
                if n * 2 > sum(c.values()):
                    out[ua] = ua + d
        # unreferenced: between two that moved by the same amount
        keys = sorted(out)
        for ua in wanted:
            if ua in out:
                continue
            i = bisect.bisect_right(keys, ua)
            if 0 < i < len(keys):
                a, b = keys[i - 1], keys[i]
                if out[a] - a == out[b] - b:
                    out[ua] = ua + out[a] - a
        return out

    def containing(self, a):
        i = bisect.bisect_right(self.us.points, a) - 1
        return self.us.points[i] if i >= 0 else a


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("rom")
    ap.add_argument("--map", nargs=2, action="append", metavar=("IN", "OUT"), required=True,
                    help="a 'name address' list of US symbols (ROM or RAM) and where to write theirs")
    ap.add_argument("--fixes", help="'name address' list of addresses found by hand")
    ap.add_argument("--guess", action="store_true",
                    help="give the symbols not found the address their predecessor's move implies "
                         "(listed as guessed in the report) instead of failing")
    ap.add_argument("--report")
    args = ap.parse_args()
    fixes = {}
    if args.fixes:
        for line in open(args.fixes):
            row = line.split("#", 1)[0].split()
            if row:
                fixes[row[0]] = int(row[1], 16)

    us = Us(args.elf)
    other = open(args.rom, "rb").read()
    lists = []
    for path, out in args.map:
        syms = []
        for line in open(path):
            name, addr = line.split()
            syms.append((name, int(addr, 16)))
        lists.append((syms, out))
    m = Mapper(us, other)
    m.run({a for syms, _ in lists for _, a in syms if not is_ram(a)})
    ram = m.map_ram({a for syms, _ in lists for _, a in syms if is_ram(a)})

    rep = sys.stderr if not args.report else open(args.report, "w")
    print(f"functions: {len(us.funcs) - len(m.unmatched_funcs)}/{len(us.funcs)} matched", file=rep)
    for a, name, n in m.unmatched_funcs:
        guess = a + m.delta_near(a)
        print(f"  func {name} {a:08x} hits={n} near={guess:08x} size={us.funcs_by_addr[a]:x}", file=rep)
    failed = False
    for syms, out in lists:
        missing = []
        with open(out, "w") as f:
            for name, a in syms:
                got = fixes.get(name)
                if got is None:
                    got = ram.get(a) if is_ram(a) else m.map.get(a)
                if got is None and args.guess and not is_ram(a):
                    missing.append((name, a))
                    got = a + m.delta_near(a)
                if got is not None:
                    f.write(f"{name} {got:08x}\n")
                else:
                    missing.append((name, a))
        print(f"{out}: {len(syms) - len(missing)}/{len(syms)} mapped", file=rep)
        for name, a in missing:
            print(f"  {'guessed' if args.guess else 'missing'} {name} {a:08x}", file=rep)
        failed |= bool(missing) and not args.guess
    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
