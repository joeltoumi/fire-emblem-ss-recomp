// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — the handful of game symbols the port layer touches.
// (Declared by hand: game headers can't be mixed with the port's.)
#ifndef PORT_GAME_H
#define PORT_GAME_H

#include <stdint.h>

typedef void (*PortGameFunc)(void);

// src/main.c — the game's entry point (never returns; SoftReset longjmps).
void AgbMain(void);

// src/irq.c — IRQ handlers, in the order of the checks in src/crt0.s.
extern PortGameFunc gIRQHandlers[14];

enum {
    INTR_SLOT_VBLANK = 0,
    INTR_SLOT_HBLANK = 1,
    INTR_SLOT_VCOUNT = 2,
};

// Unwind from deep inside the game back to main() (quit request).
_Noreturn void Port_ExitToMain(void);

#endif // PORT_GAME_H
