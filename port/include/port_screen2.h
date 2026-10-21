// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — the second screen (port/src/second_screen.c): the
// battle map, the unit under the cursor (or the battle forecast) or the army
// list, on AYN Thor's lower display or in a second window on PC. Touches on
// it move the map cursor and press the GBA buttons.
//
// Plain C, no SDL, so the headless host can test it; safe to include from C++.
#ifndef PORT_SCREEN2_H
#define PORT_SCREEN2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// What the second screen shows (PortConfig.secondScreen).
enum {
    PORT_SCREEN2_OFF,
    PORT_SCREEN2_MAP,  // the whole battle map: tap a tile to move the cursor there
    PORT_SCREEN2_UNIT, // the unit under the cursor, or the battle forecast
    PORT_SCREEN2_ARMY, // every unit on the map: tap one to put the cursor on it
    PORT_SCREEN2_COUNT
};

enum {
    PORT_TOUCH2_DOWN,
    PORT_TOUCH2_MOVE,
    PORT_TOUCH2_UP,
};

// Canvas size for a display of displayW x displayH: 256 wide, as tall as the
// display's shape allows (192..448).
void Port_SecondScreenSize(int displayW, int displayH, int* width, int* height);
// Draws the second screen in `mode` (PORT_SCREEN2_*): ABGR8888, pitch = width.
// Game thread only (it reads the game state).
void Port_SecondScreenRender(uint32_t* pixels, int width, int height, int mode);
// A touch at canvas coordinates of the last rendered picture. Returns the mode
// the screen switches to when a tab was tapped, or -1. Game thread only.
int Port_SecondScreenTouch(int x, int y, int action);
// GBA keys (PORT_KEY_*) held through the second screen's buttons.
uint16_t Port_SecondScreenKeys(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_SCREEN2_H
