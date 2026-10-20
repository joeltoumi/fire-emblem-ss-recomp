// fire-emblem-ss-recomp — RAM variables the GBA linker script places, defined
// natively by hand (tools/port/gen_ram_symbols.py generates the others).
#include "global.h"

// IRQ handlers (src/irq.c, src/crt0.s): the port's frame loop calls them.
void *gIRQHandlers[14];
