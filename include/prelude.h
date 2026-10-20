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
#if defined(PLATFORM_PC) && defined(PORT_ROM_EVENT_SCRIPTS)
// The port reads this source's event scripts from the ROM: its own
// definitions are weak placeholders that the ROM symbols override.
#define CONST_DATA __attribute__((weak))
#else
#define CONST_DATA SECTION(".data")
#endif
#ifdef PLATFORM_PC
// Code runs off the end of one overlay buffer into the next, as the GBA layout
// allows: keep them contiguous, in order, without sanitizer redzones.
#define EWRAM_OVERLAY(id) SECTION("ewram_overlay_" # id) __attribute__((no_sanitize("address")))
#else
#define EWRAM_OVERLAY(id) SECTION("ewram_overlay_" # id)
#endif

// this is for denoting objects that *should* be const, but need to not be for functions to match.
#define SHOULD_BE_CONST

#define NAKEDFUNC __attribute__((naked))

// Pointers in ROM data are 32-bit GBA addresses. The native build keeps the
// structs read from the ROM in their GBA layout: such a field is a u32, read
// with GBA_GET (data) or GBA_FUNC (a game function, through the port's table
// of GBA function addresses).
#ifdef PLATFORM_PC
void *Port_GbaFunc(u32 addr);
#define GBA_PTR(type)      u32
#define GBA_GET(type, x)   ((type)(uintptr_t)(x))
#define GBA_FUNC(type, x)  ((type)Port_GbaFunc(x))
#define GBA_SET(p)         ((u32)(uintptr_t)(p))
#else
#define GBA_PTR(type)      type
#define GBA_GET(type, x)   ((type)(x))
#define GBA_FUNC(type, x)  ((type)(x))
#define GBA_SET(p)         (p)
#endif

#endif // PRELUDE_H
