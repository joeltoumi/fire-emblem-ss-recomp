#ifndef PORT_NATIVE_H
#define PORT_NATIVE_H

/*
 * fe8 native port: force-included in every game translation unit of the
 * native build (see CMakeLists.txt).
 */

/* The decomp uses inline assembly only to make agbcc emit matching code
 * (register pinning, scheduling barriers). None of it is needed natively. */
#define asm(...)

/* agbcc knows memcpy/memset/strlen... as builtins without a declaration. */
#include <string.h>

/* Every game global goes in two sections the port can find (save states and
 * soft reset copy them as a block): __start_fe8_data/__stop_fe8_data... */
#pragma clang section bss = "fe8_bss" data = "fe8_data"

#endif /* PORT_NATIVE_H */
