#ifndef PRELUDE_H

// HACK: applying this macro seems to allow for bitpacked structs to have the same layout in agbcc and modern GCC
#define ALIGN(m) __attribute__((aligned (m)))
#define BITPACKED __attribute__((aligned(4), packed))

// this is for denoting objects that *should* be const, but weren't in the original source (resulting in them being emitted in the .data section)
#ifdef PLATFORM_PC
// Native build: the linker places everything; GBA section names only matter
// to ldscript.txt.
#define SECTION(name)
#else
#define SECTION(name) __attribute__((section(name)))
#endif
#define CONST_DATA SECTION(".data")
#define EWRAM_OVERLAY(id) SECTION("ewram_overlay_" # id)

// this is for denoting objects that *should* be const, but need to not be for functions to match.
#define SHOULD_BE_CONST

#define NAKEDFUNC __attribute__((naked))

#endif // PRELUDE_H
