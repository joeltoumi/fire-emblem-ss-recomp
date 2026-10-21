// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — widescreen (more of the battle map on the sides
// instead of bars).
//
// The game keeps running exactly as on the GBA: its camera, its 240 px
// screen and its logic don't change. Only the picture is wider: the PPU draws
// the GBA screen at column `origin` of a wider frame and keeps going on both
// sides (port/ppu: ws_origin, ws_obj, ws_world). For that to show real map:
//
//   - the game side (port/game/widescreen_game.c) hands over the whole map
//     as tilemaps for the terrain (BG3) and the movement/attack range (BG2),
//     draws the units that are in the extra columns and tags their OAM
//     entries, and tags the weather (which wraps every 256 px, so it is
//     repeated across the frame); everything else (windows, menus, the
//     cursor, effects) stays on the GBA screen;
//   - here, the frame is placed inside the map: normally the GBA screen is
//     centered, near an edge of the map the picture stops at the edge and
//     the GBA screen slides off-center, and on a map narrower than the frame
//     it is centered with black on the sides.
//
// Frames away from the battle map (title, menus, battle scenes, the world
// map, cutscenes) are drawn at 240 px and centered.
#include "port.h"

#include "../../include/port/widescreen.h"
#include "../ppu/include/cpu/mode1.h"
#include "../ppu/include/virtuappu.h"

static int sWidth = GBA_SCREEN_WIDTH;

// Reported by the game for the frame being drawn.
static bool sFrameSet;
static struct PortWideFrame sFrame;

// Placement of the last wide frame (-1: the last frame wasn't wide).
static int sOrigin = -1;

static uint32_t sWide[PORT_MAX_SCREEN_WIDTH * GBA_SCREEN_HEIGHT];

void Port_WideSetWidth(int width)
{
    if (width < GBA_SCREEN_WIDTH)
        width = GBA_SCREEN_WIDTH;
    if (width > PORT_MAX_SCREEN_WIDTH)
        width = PORT_MAX_SCREEN_WIDTH;
    width &= ~1;
    if (width != sWidth)
        sOrigin = -1;
    sWidth = width;
}

int Port_WideWidth(void)
{
    return sWidth;
}

int Port_WideLeft(void)
{
    return sOrigin >= 0 ? sOrigin : 0;
}

int Port_WideRight(void)
{
    return sOrigin >= 0 ? sWidth - GBA_SCREEN_WIDTH - sOrigin : 0;
}

void Port_WideSetFrame(const struct PortWideFrame* frame)
{
    sFrame = *frame;
    sFrameSet = true;
}

static int Clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static const uint32_t* RenderWide(void)
{
    const int w = sWidth;
    const int extra = (w - GBA_SCREEN_WIDTH) / 2;
    const int cam = sFrame.cameraX;
    // The part of the map that may be shown always contains the GBA screen.
    const int left = sFrame.left < cam ? sFrame.left : cam;
    const int right = sFrame.right > cam + GBA_SCREEN_WIDTH ? sFrame.right : cam + GBA_SCREEN_WIDTH;
    int origin;

    if (right - left >= w)
        origin = Clamp(extra, cam + w - right, cam - left);
    else
        origin = cam - left + (w - (right - left)) / 2; // narrow: center it
    origin = Clamp(origin, 0, 2 * extra);
    sOrigin = origin;

    const int visL = Clamp(left - cam + origin, 0, w);
    const int visR = Clamp(right - cam + origin, visL, w);

    virtuappu_mode1_ws_active = 1;
    virtuappu_mode1_ws_origin = origin;
    virtuappu_mode1_ws_bg_clip = 0;
    for (int i = 0; i < 4; i++) {
        const struct PortWideBg* bg = &sFrame.bg[i];
        VirtuaPPUMode1WsWorld* world = &virtuappu_mode1_ws_world[i];
        world->map = bg->map;
        world->w = bg->w;
        world->h = bg->h;
        world->x = bg->x;
        world->y = bg->y;
        world->reg_x = bg->regX;
        world->reg_y = bg->regY;
        if (bg->map == NULL)
            virtuappu_mode1_ws_bg_clip |= 1 << i;
    }
    for (int i = 0; i < 128; i++) {
        VirtuaPPUMode1WsObj* o = &virtuappu_mode1_ws_obj[i];
        o->shift = (int16_t)origin;
        o->repeat = 0;
        if (sFrame.oamTags[i] == PORT_WIDE_OAM_WORLD) {
            o->wrap = (int16_t)(w - origin);
            o->clip_l = (int16_t)visL;
            o->clip_r = (int16_t)visR;
        } else if (sFrame.oamTags[i] == PORT_WIDE_OAM_WEATHER) {
            o->wrap = 256;
            o->repeat = 256;
            o->clip_l = (int16_t)visL;
            o->clip_r = (int16_t)visR;
        } else {
            o->wrap = GBA_SCREEN_WIDTH;
            o->clip_l = (int16_t)origin;
            o->clip_r = (int16_t)(origin + GBA_SCREEN_WIDTH);
        }
    }

    uint32_t* frame = (uint32_t*)Port_PpuRenderFrame(w);
    virtuappu_mode1_ws_active = 0;
    virtuappu_mode1_ws_bg_clip = 0;
    for (int i = 0; i < 4; i++)
        virtuappu_mode1_ws_world[i].map = NULL;

    // Outside the map: black, like the bars it replaces.
    if (visL > 0 || visR < w) {
        for (int y = 0; y < GBA_SCREEN_HEIGHT; y++) {
            uint32_t* row = frame + (size_t)y * w;
            for (int x = 0; x < visL; x++)
                row[x] = 0xFF000000u;
            for (int x = visR; x < w; x++)
                row[x] = 0xFF000000u;
        }
    }
    return frame;
}

const uint32_t* Port_WideRenderFrame(int* width)
{
    *width = sWidth;
    if (sWidth == GBA_SCREEN_WIDTH) {
        sOrigin = -1;
        return Port_PpuRenderFrame(GBA_SCREEN_WIDTH);
    }

    sFrameSet = false;
    Port_WideGameFrame();
    if (sFrameSet)
        return RenderWide();

    // Away from the battle map: the GBA picture in the middle.
    sOrigin = -1;
    const uint32_t* gba = Port_PpuRenderFrame(GBA_SCREEN_WIDTH);
    const int extra = (sWidth - GBA_SCREEN_WIDTH) / 2;
    for (int y = 0; y < GBA_SCREEN_HEIGHT; y++) {
        uint32_t* row = sWide + (size_t)y * sWidth;
        for (int x = 0; x < extra; x++)
            row[x] = row[sWidth - 1 - x] = 0xFF000000u;
        memcpy(row + extra, gba + (size_t)y * GBA_SCREEN_WIDTH, GBA_SCREEN_WIDTH * sizeof(uint32_t));
    }
    return sWide;
}
