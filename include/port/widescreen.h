#ifndef PORT_WIDESCREEN_STATE_H
#define PORT_WIDESCREEN_STATE_H

/*
 * fire-emblem-ss-recomp: widescreen on the battle map, between the game side
 * (port/game/widescreen_game.c) and the port side (port/src/widescreen.c).
 *
 * The game keeps its 240 px screen, its camera and its logic. Before each
 * frame is drawn, the game side hands the port the whole map as tilemaps
 * (the game's own tilemaps are rings that only hold the 240 px screen) and
 * which OAM entries are map objects (units), and the port draws the GBA
 * screen inside a wider frame with more of the map on both sides.
 *
 * Shared by game code and port code: plain C types only.
 */

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PORT_WIDE_OAM_SCREEN, /* drawn like on the GBA, clipped to its screen */
    PORT_WIDE_OAM_WORLD,  /* a map object: visible in the extra columns too */
    PORT_WIDE_OAM_WEATHER, /* weather that wraps every 256 px: fills the frame */
};

struct PortWideBg {
    const unsigned short* map; /* NULL: this BG stays on the GBA screen */
    int w, h;                  /* 8 px tiles */
    int x, y;                  /* map pixel at the GBA screen's top-left... */
    int regX, regY;            /* ...while the BG scroll registers are these */
};

struct PortWideFrame {
    unsigned char oamTags[128]; /* PORT_WIDE_OAM_* */
    struct PortWideBg bg[4];
    int cameraX;     /* map pixel at the GBA screen's left edge */
    int left, right; /* map pixels the picture may show: [left, right) */
};

/* Port side. Frame width (240: widescreen off). */
int Port_WideWidth(void);
/* Columns the last wide frame had left and right of the GBA screen (0 when
 * it wasn't wide). The game draws the units that are in them. */
int Port_WideLeft(void);
int Port_WideRight(void);
/* This frame is a wide one (the battle map is on screen). */
void Port_WideSetFrame(const struct PortWideFrame* frame);

/* Game side. Called by the port before it draws each frame, when the
 * width is above 240. */
void Port_WideGameFrame(void);
/* OAM bookkeeping: entries put while gPortWideOamTag is set get that tag
 * (PORT_WIDE_OAM_*); SyncHiOam/SyncLoOam latch the buffer's tags for the
 * frame. */
extern int gPortWideOamTag;
void Port_WideTagOam(const void* entry, int count);
void Port_WideOamSynced(const void* buffer, int count);

#ifdef __cplusplus
}
#endif

#endif /* PORT_WIDESCREEN_STATE_H */
