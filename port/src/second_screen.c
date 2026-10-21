// SPDX-License-Identifier: GPL-3.0-or-later
// fire-emblem-ss-recomp — the second screen (AYN Thor's lower display, or a
// second window on PC): the battle map, the unit under the cursor or the
// battle forecast, and the army list, with buttons for the GBA keys.
//
// Drawn in software into a small canvas (256 pixels wide) that the host
// scales to the display. The game state comes from port/game/screen2_game.c
// (include/port/screen2.h): the map is drawn with the chapter's own tiles and
// the units with their standing sprites. Text uses an 8x8 bitmap font.
//
// Touches: a tap on a map tile moves the cursor there (a second tap on the
// cursor presses A), a tap on a unit of the army list puts the cursor on it,
// and the tabs switch what the screen shows.
#include "port.h"
#include "port_screen2.h"
#include "../../include/port/screen2.h"
#include "font8x8.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define HEADER_H 16
#define FOOTER_H 22
#define INFO_H 28
#define ROW_H 14

static int MaxI(int a, int b) { return a > b ? a : b; }
static int MinI(int a, int b) { return a < b ? a : b; }
static int ClampI(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static uint32_t* sPx;
static int sW, sH;

static uint32_t Rgb(int r, int g, int b)
{
    return 0xFF000000u | (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r;
}

#define COLOR_BG Rgb(14, 16, 30)
#define COLOR_PANEL Rgb(30, 38, 78)
#define COLOR_PANEL_HI Rgb(58, 74, 140)
#define COLOR_EDGE Rgb(150, 130, 70)
#define COLOR_TEXT Rgb(240, 240, 248)
#define COLOR_DIM Rgb(130, 136, 160)
#define COLOR_GOLD Rgb(250, 210, 90)
#define COLOR_BLUE Rgb(110, 170, 255)
#define COLOR_RED Rgb(255, 110, 100)
#define COLOR_GREEN Rgb(110, 230, 120)
#define COLOR_PURPLE Rgb(200, 130, 255)
#define COLOR_HP Rgb(120, 230, 110)
#define COLOR_HP_LOW Rgb(250, 90, 70)
#define COLOR_HP_MID Rgb(250, 200, 70)
#define COLOR_LOSS Rgb(90, 30, 30)

static uint32_t FactionColor(int faction)
{
    switch (faction) {
    case PORT_S2_FACTION_RED:
        return COLOR_RED;
    case PORT_S2_FACTION_GREEN:
        return COLOR_GREEN;
    case PORT_S2_FACTION_PURPLE:
        return COLOR_PURPLE;
    case PORT_S2_FACTION_OBJECT:
        return COLOR_DIM;
    default:
        return COLOR_BLUE;
    }
}

static void Fill(int x, int y, int w, int h, uint32_t c)
{
    for (int j = MaxI(y, 0); j < y + h && j < sH; j++) {
        for (int i = MaxI(x, 0); i < x + w && i < sW; i++)
            sPx[j * sW + i] = c;
    }
}

static void Frame(int x, int y, int w, int h, int t, uint32_t c)
{
    Fill(x, y, w, t, c);
    Fill(x, y + h - t, w, t, c);
    Fill(x, y, t, h, c);
    Fill(x + w - t, y, t, h, c);
}

static int TextWidth(const char* s, int scale)
{
    return (int)strlen(s) * 8 * scale;
}

// Draws at most `maxW` pixels of `s` (whole characters).
static int TextClip(int x, int y, int scale, uint32_t c, const char* s, int maxW)
{
    const int x0 = x;
    for (; *s && x + 8 * scale <= x0 + maxW; s++, x += 8 * scale) {
        const unsigned ch = (unsigned char)*s;
        if (ch < PORT_FONT_FIRST || ch > PORT_FONT_LAST)
            continue;
        const uint8_t* glyph = &kPortFont8x8[(ch - PORT_FONT_FIRST) * 8];
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (glyph[row] & (1 << col))
                    Fill(x + col * scale, y + row * scale, scale, scale, c);
            }
        }
    }
    return x - x0;
}

static int Text(int x, int y, uint32_t c, const char* s)
{
    return TextClip(x, y, 1, c, s, 4096);
}

static void TextRight(int right, int y, uint32_t c, const char* s)
{
    Text(right - TextWidth(s, 1), y, c, s);
}

static void Upper(char* dst, const char* src, size_t size)
{
    size_t i = 0;
    for (; src[i] && i + 1 < size; i++)
        dst[i] = (src[i] >= 'a' && src[i] <= 'z') ? (char)(src[i] - 32) : src[i];
    dst[i] = 0;
}

static void HpBar(int x, int y, int w, int h, int hp, int maxHp, int loss)
{
    Fill(x, y, w, h, Rgb(20, 20, 30));
    if (maxHp <= 0)
        return;
    const int left = MaxI(hp - loss, 0);
    const int fw = (w * left + maxHp - 1) / maxHp, lw = (w * hp + maxHp - 1) / maxHp;
    const uint32_t c = hp * 4 <= maxHp ? COLOR_HP_LOW : hp * 2 <= maxHp ? COLOR_HP_MID : COLOR_HP;
    Fill(x, y, MinI(lw, w), h, COLOR_LOSS);
    Fill(x, y, MinI(fw, w), h, c);
}

// ---- touch regions ------------------------------------------------------------

enum {
    HIT_TAB,
    HIT_KEY,
    HIT_MAP,
    HIT_UNIT,   // a = x, b = y of the unit
    HIT_FILTER, // a = army list filter
    HIT_LIST,   // drag to scroll
};

typedef struct Hit {
    int x, y, w, h;
    int kind, a, b;
} Hit;

static Hit sHits[96];
static int sHitCount;

static void AddHit(int x, int y, int w, int h, int kind, int a, int b)
{
    if (sHitCount < (int)(sizeof(sHits) / sizeof(sHits[0])))
        sHits[sHitCount++] = (Hit){ x, y, w, h, kind, a, b };
}

// The map as last drawn: origin, tile size and the first tile shown.
static struct {
    int x, y, w, h;
    float ts;
    int viewX, viewY;
} sMapView;

static int sArmyFilter; // 0 allies, 1 enemies, 2 others
static int sArmyScroll; // pixels
static const struct PortS2State* sState;

// ---- header and footer --------------------------------------------------------

static const char* const kTabNames[] = { "", "MAP", "UNIT", "ARMY" };

static void Header(int mode)
{
    char line[32];

    Fill(0, 0, sW, HEADER_H, COLOR_PANEL);
    Fill(0, HEADER_H - 1, sW, 1, COLOR_EDGE);
    for (int m = PORT_SCREEN2_MAP; m < PORT_SCREEN2_COUNT; m++) {
        const int x = (m - 1) * 48, w = 46;
        if (m == mode)
            Fill(x, 0, w, HEADER_H - 1, COLOR_PANEL_HI);
        Text(x + (w - TextWidth(kTabNames[m], 1)) / 2, 4, m == mode ? COLOR_GOLD : COLOR_DIM, kTabNames[m]);
        AddHit(x, 0, w, HEADER_H, HIT_TAB, m, 0);
    }
    if (sState->onMap) {
        snprintf(line, sizeof(line), "TURN %d", sState->turn);
        TextRight(sW - 14, 4, COLOR_TEXT, line);
        Fill(sW - 10, 4, 7, 7, FactionColor(sState->phase));
    }
}

static const struct {
    const char* label;
    uint16_t key;
} kButtons[] = {
    { "B", PORT_KEY_B },       { "A", PORT_KEY_A },     { "L", PORT_KEY_L },
    { "R", PORT_KEY_R },       { "SEL", PORT_KEY_SELECT }, { "START", PORT_KEY_START },
};
#define BUTTON_COUNT ((int)(sizeof(kButtons) / sizeof(kButtons[0])))

static uint16_t sHeldKeys;     // keys held by fingers on the buttons
static uint16_t sTapKeys;      // keys still held for a short tap
static uint64_t sTapUntil[16]; // frame until which a tapped key stays held

static void Footer(void)
{
    const int y = sH - FOOTER_H;
    const uint16_t held = Port_SecondScreenKeys();

    Fill(0, y, sW, FOOTER_H, COLOR_PANEL);
    Fill(0, y, sW, 1, COLOR_EDGE);
    for (int i = 0; i < BUTTON_COUNT; i++) {
        const int x0 = i * sW / BUTTON_COUNT, x1 = (i + 1) * sW / BUTTON_COUNT;
        const bool down = (held & kButtons[i].key) != 0;
        Fill(x0 + 2, y + 3, x1 - x0 - 4, FOOTER_H - 5, down ? COLOR_GOLD : COLOR_PANEL_HI);
        Text((x0 + x1 - TextWidth(kButtons[i].label, 1)) / 2, y + 7, down ? COLOR_PANEL : COLOR_TEXT,
             kButtons[i].label);
        AddHit(x0, y, x1 - x0, FOOTER_H, HIT_KEY, kButtons[i].key, 0);
    }
}

// ---- map ----------------------------------------------------------------------

static uint32_t* sMapCache;
static int sMapCacheW, sMapCacheH;
static unsigned sMapCacheVersion;
static float sMapCacheTs;
static int sMapCacheX = -1, sMapCacheY = -1;

// The part of the map in view, scaled to `ts` pixels per tile (box filter when
// shrinking), into sMapCache.
static void ScaleMap(const struct PortS2State* s, float ts, int vx, int vy, int dw, int dh)
{
    if (sMapCache && sMapCacheVersion == s->mapVersion && sMapCacheTs == ts && sMapCacheX == vx &&
        sMapCacheY == vy && sMapCacheW == dw && sMapCacheH == dh)
        return;
    free(sMapCache);
    sMapCache = malloc((size_t)dw * dh * sizeof(uint32_t));
    if (!sMapCache)
        return;
    sMapCacheVersion = s->mapVersion;
    sMapCacheTs = ts;
    sMapCacheX = vx;
    sMapCacheY = vy;
    sMapCacheW = dw;
    sMapCacheH = dh;

    const int pitch = s->mapW * 16, srcW = s->mapW * 16, srcH = s->mapH * 16;
    const float step = 16.0f / ts;
    for (int dy = 0; dy < dh; dy++) {
        const int y0 = ClampI(vy * 16 + (int)(dy * step), 0, srcH - 1);
        const int y1 = ClampI(MaxI(vy * 16 + (int)((dy + 1) * step), y0 + 1), 1, srcH);
        for (int dx = 0; dx < dw; dx++) {
            const int x0 = ClampI(vx * 16 + (int)(dx * step), 0, srcW - 1);
            const int x1 = ClampI(MaxI(vx * 16 + (int)((dx + 1) * step), x0 + 1), 1, srcW);
            unsigned r = 0, g = 0, b = 0, n = 0;
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    const uint32_t p = s->mapPx[y * pitch + x];
                    r += p & 0xFF;
                    g += (p >> 8) & 0xFF;
                    b += (p >> 16) & 0xFF;
                    n++;
                }
            }
            sMapCache[dy * dw + dx] = n ? Rgb(r / n, g / n, b / n) : 0xFF000000u;
        }
    }
}

static void Sprite(const struct PortS2Unit* u, int tileX, int tileY, float ts)
{
    // The standing animation: long rests between short steps (like the map's).
    static const uint8_t anim[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1 };
    const int frame = anim[(Port_FrameCount() / 4) % sizeof(anim)];
    const int palette = (u->acted && u->faction == PORT_S2_FACTION_BLUE) ? 4 : u->faction;
    int w, h;
    const unsigned* px = Port_S2Sprite(u->sms, palette, frame, &w, &h);

    if (!px) {
        Fill(tileX + (int)(ts * 0.25f), tileY + (int)(ts * 0.25f), (int)(ts * 0.5f), (int)(ts * 0.5f),
             FactionColor(u->faction));
        return;
    }
    const float scale = ts / 16.0f;
    const int dw = (int)(w * scale + 0.5f), dh = (int)(h * scale + 0.5f);
    const int ox = tileX + ((int)ts - dw) / 2, oy = tileY + (int)ts - dh;
    for (int y = 0; y < dh; y++) {
        const int sy = MinI((int)(y / scale), h - 1);
        if (oy + y < HEADER_H || oy + y >= sH)
            continue;
        for (int x = 0; x < dw; x++) {
            const int sx = MinI((int)(x / scale), w - 1);
            const uint32_t p = px[sy * w + sx];
            if (p && ox + x >= 0 && ox + x < sW)
                sPx[(oy + y) * sW + ox + x] = p;
        }
    }
}

// Map in the rectangle (x, y, w, h).
static void MapView(const struct PortS2State* s, int ax, int ay, int aw, int ah)
{
    float ts = fminf((float)aw / s->mapW, (float)ah / s->mapH);
    int viewW = s->mapW, viewH = s->mapH, vx = 0, vy = 0;

    if (ts >= 32)
        ts = 32;
    else if (ts > 16)
        ts = 16;
    if (ts < 6) {
        // Too big to show whole: the part around the main screen.
        ts = 6;
        viewW = MinI(s->mapW, aw / 6);
        viewH = MinI(s->mapH, ah / 6);
        vx = ClampI((s->cameraX + 120) / 16 - viewW / 2, 0, s->mapW - viewW);
        vy = ClampI((s->cameraY + 80) / 16 - viewH / 2, 0, s->mapH - viewH);
    }
    const int dw = (int)(viewW * ts), dh = (int)(viewH * ts);
    const int ox = ax + (aw - dw) / 2, oy = ay + (ah - dh) / 2;

    sMapView.x = ox;
    sMapView.y = oy;
    sMapView.w = dw;
    sMapView.h = dh;
    sMapView.ts = ts;
    sMapView.viewX = vx;
    sMapView.viewY = vy;
    AddHit(ox, oy, dw, dh, HIT_MAP, 0, 0);

    ScaleMap(s, ts, vx, vy, dw, dh);
    if (sMapCache) {
        for (int y = 0; y < dh; y++)
            memcpy(&sPx[(oy + y) * sW + ox], &sMapCache[y * dw], (size_t)dw * sizeof(uint32_t));
    }

    // What the main screen shows.
    if (s->mapW > 15 || s->mapH > 10) {
        const int cx = ox + (int)((s->cameraX / 16.0f - vx) * ts), cy = oy + (int)((s->cameraY / 16.0f - vy) * ts);
        Frame(cx, cy, (int)(15 * ts), (int)(10 * ts), 1, Rgb(255, 230, 140));
    }

    for (int i = 0; i < s->unitCount; i++) {
        const struct PortS2Unit* u = &s->units[i];
        if (u->x < vx || u->y < vy || u->x >= vx + viewW || u->y >= vy + viewH)
            continue;
        const int tx = ox + (int)((u->x - vx) * ts), ty = oy + (int)((u->y - vy) * ts);
        Sprite(u, tx, ty, ts);
        if (ts >= 10 && u->hp < u->maxHp)
            HpBar(tx + 1, ty + (int)ts - 2, (int)ts - 2, 2, u->hp, u->maxHp, 0);
        if (u->boss && ts >= 10)
            Fill(tx + (int)ts - 4, ty + 1, 3, 3, COLOR_GOLD);
    }

    // The cursor, blinking.
    if ((Port_FrameCount() / 16) % 4 != 3 && s->cursorX >= vx && s->cursorY >= vy && s->cursorX < vx + viewW &&
        s->cursorY < vy + viewH) {
        const int x = ox + (int)((s->cursorX - vx) * ts), y = oy + (int)((s->cursorY - vy) * ts);
        const int t = ts >= 12 ? 2 : 1;
        Frame(x, y, (int)ts, (int)ts, t, Rgb(255, 255, 255));
    }
}

static void FighterLine(int x, int y, int w, const struct PortS2Fighter* f)
{
    char line[48];

    TextClip(x, y, 1, FactionColor(f->faction), f->name, 80);
    char mt[8] = "  ";
    if (f->hits > 1)
        snprintf(mt, sizeof(mt), "x%d", f->hits);
    if (f->mt < 0)
        snprintf(line, sizeof(line), "HP%2d  --    --   --", f->hp);
    else
        snprintf(line, sizeof(line), "HP%2d %3d%s %3d%% %3d%%", f->hp, f->mt, mt, f->hit, f->crit);
    TextRight(x + w, y, COLOR_TEXT, line);
}

static void MapInfo(const struct PortS2State* s, int y)
{
    char line[48];

    Fill(0, y, sW, INFO_H, COLOR_PANEL);
    Fill(0, y, sW, 1, COLOR_EDGE);
    if (s->forecast.valid) {
        FighterLine(4, y + 5, sW - 8, &s->forecast.attacker);
        FighterLine(4, y + 16, sW - 8, &s->forecast.defender);
        return;
    }
    if (s->detail.valid) {
        const struct PortS2Unit* u = &s->detail.unit;
        const int nw = TextClip(4, y + 5, 1, FactionColor(u->faction), u->name, 104);
        snprintf(line, sizeof(line), " LV%d", u->level);
        Text(4 + nw, y + 5, COLOR_DIM, line);
        snprintf(line, sizeof(line), "HP %2d/%2d", u->hp, u->maxHp);
        Text(4, y + 16, COLOR_TEXT, line);
        HpBar(4 + 72, y + 18, 44, 4, u->hp, u->maxHp, 0);
        for (int i = 0; i < s->detail.itemCount; i++) {
            if (s->detail.items[i].equipped)
                TextClip(sW - 4 - 128, y + 16, 1, COLOR_GOLD, s->detail.items[i].name, 128);
        }
        TextClip(sW - 4 - 128, y + 5, 1, COLOR_DIM, u->className, 128);
        return;
    }
    TextClip(4, y + 5, 1, COLOR_GOLD, s->chapter, sW - 8);
    snprintf(line, sizeof(line), "%s", s->terrain);
    Text(4, y + 16, COLOR_TEXT, line);
    snprintf(line, sizeof(line), "%dG", s->gold);
    TextRight(sW - 4, y + 16, COLOR_DIM, line);
}

static void DrawMap(const struct PortS2State* s)
{
    const int top = HEADER_H, bottom = sH - FOOTER_H - INFO_H;
    MapView(s, 0, top + 1, sW, bottom - top - 2);
    MapInfo(s, bottom);
}

// ---- unit / forecast ----------------------------------------------------------

static void BigSprite(const struct PortS2Unit* u, int x, int y)
{
    Sprite(u, x, y, 32);
}

static void ForecastSide(const struct PortS2Fighter* f, const struct PortS2Fighter* foe, int x, int y, int w)
{
    char line[32];
    const uint32_t c = FactionColor(f->faction);
    const int loss = foe->mt > 0 && foe->hits > 0 ? foe->mt * foe->hits : 0;

    Fill(x, y, w, 2, c);
    TextClip(x + (w - MinI(TextWidth(f->name, 1), w)) / 2, y + 6, 1, c, f->name, w);
    TextClip(x + (w - MinI(TextWidth(f->weapon, 1), w)) / 2, y + 18, 1, COLOR_DIM, f->weapon, w);
    snprintf(line, sizeof(line), "%d", f->hp);
    TextClip(x + (w - TextWidth(line, 2)) / 2, y + 32, 2, COLOR_TEXT, line, w);
    HpBar(x + 8, y + 52, w - 16, 5, f->hp, MaxI(f->maxHp, f->hp), loss);
    snprintf(line, sizeof(line), "-> %d", MaxI(f->hp - loss, 0));
    TextClip(x + (w - TextWidth(line, 1)) / 2, y + 61, 1, f->hp - loss <= 0 ? COLOR_HP_LOW : COLOR_DIM, line, w);

    const char* labels[] = { "DMG", "HIT", "CRIT" };
    const int values[] = { f->mt, f->hit, f->crit };
    for (int i = 0; i < 3; i++) {
        const int ly = y + 78 + i * 13;
        Text(x + 8, ly, COLOR_GOLD, labels[i]);
        if (values[i] < 0)
            snprintf(line, sizeof(line), "--");
        else
            snprintf(line, sizeof(line), i == 0 && f->hits > 1 ? "%d x%d" : "%d", values[i], f->hits);
        TextRight(x + w - 8, ly, COLOR_TEXT, line);
    }
    if (f->effective)
        TextClip(x + (w - TextWidth("EFFECTIVE", 1)) / 2, y + 120, 1, COLOR_GREEN, "EFFECTIVE", w);
}

static void DrawForecast(const struct PortS2State* s, int top, int bottom)
{
    const int w = (sW - 12) / 2;
    const int y = top + MaxI(4, (bottom - top - 132) / 2);
    Text((sW - TextWidth("BATTLE FORECAST", 1)) / 2, MaxI(top + 3, y - 12), COLOR_GOLD, "BATTLE FORECAST");
    ForecastSide(&s->forecast.attacker, &s->forecast.defender, 4, y, w);
    ForecastSide(&s->forecast.defender, &s->forecast.attacker, sW - 4 - w, y, w);
    Fill(sW / 2, y + 6, 1, 120, COLOR_PANEL_HI);
}

static void Stat(int x, int y, const char* label, int value)
{
    char num[8];
    Text(x, y, COLOR_GOLD, label);
    snprintf(num, sizeof(num), "%d", value);
    TextRight(x + 56, y, COLOR_TEXT, num);
}

static void DrawUnit(const struct PortS2State* s)
{
    const int top = HEADER_H + 2, bottom = sH - FOOTER_H;
    char line[48];

    if (!s->onMap)
        return;
    if (s->forecast.valid) {
        DrawForecast(s, top, bottom);
        return;
    }
    if (!s->detail.valid) {
        const char* t = "NO UNIT HERE";
        Text((sW - TextWidth(t, 1)) / 2, (top + bottom) / 2 - 12, COLOR_DIM, t);
        if (s->terrain[0])
            Text((sW - TextWidth(s->terrain, 1)) / 2, (top + bottom) / 2 + 4, COLOR_TEXT, s->terrain);
        return;
    }

    const struct PortS2Detail* d = &s->detail;
    const struct PortS2Unit* u = &d->unit;
    const int y0 = top + MaxI(2, (bottom - top - 190) / 3);

    BigSprite(u, 8, y0 + 2);
    TextClip(48, y0 + 4, 1, FactionColor(u->faction), u->name, sW - 52);
    TextClip(48, y0 + 15, 1, COLOR_DIM, u->className, sW - 52);
    snprintf(line, sizeof(line), "LV %d", u->level);
    Text(48, y0 + 26, COLOR_TEXT, line);
    if (u->faction == PORT_S2_FACTION_BLUE) {
        snprintf(line, sizeof(line), "EXP %d", d->exp);
        Text(48 + 56, y0 + 26, COLOR_TEXT, line);
    }
    if (d->status[0]) {
        char status[PORT_S2_NAME_LEN];
        Upper(status, d->status, sizeof(status));
        TextRight(sW - 6, y0 + 26, COLOR_PURPLE, status);
    }

    int y = y0 + 40;
    snprintf(line, sizeof(line), "HP %d/%d", u->hp, u->maxHp);
    Text(8, y, COLOR_TEXT, line);
    HpBar(8 + TextWidth(line, 1) + 8, y + 2, sW - 24 - TextWidth(line, 1), 4, u->hp, u->maxHp, 0);
    y += 14;

    const int cw = (sW - 16) / 3;
    Stat(8, y, d->magic ? "MAG" : "STR", d->pow);
    Stat(8 + cw, y, "SKL", d->skl);
    Stat(8 + 2 * cw, y, "SPD", d->spd);
    y += 11;
    Stat(8, y, "LCK", d->lck);
    Stat(8 + cw, y, "DEF", d->def);
    Stat(8 + 2 * cw, y, "RES", d->res);
    y += 11;
    Stat(8, y, "MOV", d->mov);
    Stat(8 + cw, y, "CON", d->con);
    Stat(8 + 2 * cw, y, "AID", d->aid);
    y += 15;

    Fill(8, y - 3, sW - 16, 1, COLOR_PANEL_HI);
    for (int i = 0; i < d->itemCount; i++, y += 11) {
        const struct PortS2Item* it = &d->items[i];
        TextClip(8, y, 1, it->equipped ? COLOR_GOLD : COLOR_TEXT, it->name, sW - 64);
        if (it->maxUses)
            snprintf(line, sizeof(line), "%d/%d", it->uses, it->maxUses);
        else
            snprintf(line, sizeof(line), "--");
        TextRight(sW - 8, y, COLOR_DIM, line);
    }
    if (s->terrain[0] && y + 12 < bottom)
        TextRight(sW - 8, bottom - 12, COLOR_DIM, s->terrain);
}

// ---- army -------------------------------------------------------------------------

static bool InFilter(const struct PortS2Unit* u, int filter)
{
    switch (filter) {
    case 0:
        return u->faction == PORT_S2_FACTION_BLUE;
    case 1:
        return u->faction == PORT_S2_FACTION_RED || u->faction == PORT_S2_FACTION_PURPLE;
    default:
        return u->faction == PORT_S2_FACTION_GREEN;
    }
}

static void DrawArmy(const struct PortS2State* s)
{
    static const char* const names[] = { "ALLIES", "ENEMIES", "OTHERS" };
    const int top = HEADER_H, bottom = sH - FOOTER_H;
    int counts[3] = { 0, 0, 0 };
    char line[48];

    if (!s->onMap)
        return;
    for (int i = 0; i < s->unitCount; i++) {
        for (int f = 0; f < 3; f++)
            counts[f] += InFilter(&s->units[i], f);
    }
    if (sArmyFilter == 2 && counts[2] == 0)
        sArmyFilter = 0;

    // Filter chips.
    int x = 4;
    for (int f = 0; f < 3; f++) {
        if (f == 2 && counts[2] == 0)
            continue;
        snprintf(line, sizeof(line), "%s %d", names[f], counts[f]);
        const int w = TextWidth(line, 1) + 10;
        Fill(x, top + 3, w, 13, f == sArmyFilter ? COLOR_PANEL_HI : COLOR_PANEL);
        Text(x + 5, top + 6, f == sArmyFilter ? COLOR_GOLD : COLOR_DIM, line);
        AddHit(x, top, w, 18, HIT_FILTER, f, 0);
        x += w + 4;
    }

    const int listTop = top + 19, listH = bottom - listTop;
    const int total = counts[sArmyFilter] * ROW_H;
    sArmyScroll = ClampI(sArmyScroll, 0, MaxI(total - listH, 0));
    AddHit(0, listTop, sW, listH, HIT_LIST, 0, 0);

    int row = 0;
    for (int i = 0; i < s->unitCount; i++) {
        const struct PortS2Unit* u = &s->units[i];
        if (!InFilter(u, sArmyFilter))
            continue;
        const int y = listTop + row * ROW_H - sArmyScroll;
        row++;
        if (y + ROW_H <= listTop || y >= bottom)
            continue;
        const bool atCursor = u->x == s->cursorX && u->y == s->cursorY;
        const int clipTop = MaxI(y, listTop), clipBottom = MinI(y + ROW_H, bottom);
        if (atCursor)
            Fill(0, clipTop, sW, clipBottom - clipTop, COLOR_PANEL_HI);
        if (y >= listTop && y + ROW_H <= bottom) {
            const uint32_t c = u->acted ? COLOR_DIM : FactionColor(u->faction);
            Fill(4, y + 4, 5, 5, c);
            TextClip(12, y + 3, 1, u->acted ? COLOR_DIM : COLOR_TEXT, u->name, 96);
            snprintf(line, sizeof(line), "%d", u->level);
            TextRight(124, y + 3, COLOR_DIM, line);
            snprintf(line, sizeof(line), "%2d/%2d", u->hp, u->maxHp);
            Text(132, y + 3, COLOR_TEXT, line);
            HpBar(176, y + 5, sW - 182, 4, u->hp, u->maxHp, 0);
            if (u->boss)
                Fill(sW - 4, y + 4, 2, 5, COLOR_GOLD);
        }
        AddHit(0, clipTop, sW, clipBottom - clipTop, HIT_UNIT, u->x, u->y);
    }
    if (total > listH) {
        // Scroll bar.
        const int bh = MaxI(8, listH * listH / total), by = listTop + (listH - bh) * sArmyScroll / (total - listH);
        Fill(sW - 2, by, 2, bh, COLOR_DIM);
    }
}

// ---- interface --------------------------------------------------------------------

void Port_SecondScreenSize(int displayW, int displayH, int* width, int* height)
{
    int h = displayW > 0 ? (int)((int64_t)256 * displayH / displayW) : 224;
    *width = 256;
    *height = h < 192 ? 192 : h > 448 ? 448 : h;
}

void Port_SecondScreenRender(uint32_t* pixels, int width, int height, int mode)
{
    sState = Port_S2Snapshot();
    sPx = pixels;
    sW = width;
    sH = height;
    sHitCount = 0;
    Fill(0, 0, width, height, COLOR_BG);

    if (mode <= PORT_SCREEN2_OFF || mode >= PORT_SCREEN2_COUNT)
        mode = PORT_SCREEN2_MAP;
    Header(mode);
    if (!sState->onMap) {
        // Title, world map, menus, cutscenes...
        const char* t = "FIRE EMBLEM";
        TextClip((width - TextWidth(t, 2)) / 2, height / 2 - 26, 2, COLOR_GOLD, t, width);
        t = "THE SACRED STONES";
        Text((width - TextWidth(t, 1)) / 2, height / 2 - 4, COLOR_DIM, t);
    } else if (mode == PORT_SCREEN2_UNIT) {
        DrawUnit(sState);
    } else if (mode == PORT_SCREEN2_ARMY) {
        DrawArmy(sState);
    } else {
        DrawMap(sState);
    }
    Footer();
}

static const Hit* HitAt(int x, int y)
{
    for (int i = sHitCount - 1; i >= 0; i--) {
        const Hit* h = &sHits[i];
        if (x >= h->x && y >= h->y && x < h->x + h->w && y < h->y + h->h)
            return h;
    }
    return NULL;
}

static Hit sTouchStart; // what the finger went down on
static bool sTouching;
static int sTouchX, sTouchY, sTouchScroll;
static bool sDragged;

static void TapKey(uint16_t key)
{
    for (int b = 0; b < 16; b++) {
        if (key & (1 << b))
            sTapUntil[b] = Port_FrameCount() + 3;
    }
    sTapKeys |= key;
}

int Port_SecondScreenTouch(int x, int y, int action)
{
    int newMode = -1;

    if (action == PORT_TOUCH2_DOWN) {
        const Hit* h = HitAt(x, y);
        sTouching = h != NULL;
        if (!h)
            return -1;
        sTouchStart = *h;
        sTouchX = x;
        sTouchY = y;
        sTouchScroll = sArmyScroll;
        sDragged = false;
        if (h->kind == HIT_KEY) {
            sHeldKeys |= (uint16_t)h->a;
            TapKey((uint16_t)h->a);
        } else if (h->kind == HIT_TAB) {
            newMode = h->a;
        } else if (h->kind == HIT_FILTER) {
            sArmyFilter = h->a;
            sArmyScroll = 0;
        }
        return newMode;
    }
    if (!sTouching)
        return -1;
    if (abs(y - sTouchY) > 6 || abs(x - sTouchX) > 6)
        sDragged = true;
    if (action == PORT_TOUCH2_MOVE) {
        if (sTouchStart.kind == HIT_UNIT || sTouchStart.kind == HIT_LIST)
            sArmyScroll = MaxI(0, sTouchScroll - (y - sTouchY));
        return -1;
    }

    // Finger up.
    sTouching = false;
    if (sTouchStart.kind == HIT_KEY) {
        sHeldKeys &= (uint16_t)~sTouchStart.a;
    } else if (sTouchStart.kind == HIT_UNIT && !sDragged) {
        Port_S2MoveCursor(sTouchStart.a, sTouchStart.b);
    } else if (sTouchStart.kind == HIT_MAP && !sDragged && sState && sState->onMap && sMapView.ts > 0) {
        const int tx = sMapView.viewX + (int)((sTouchX - sMapView.x) / sMapView.ts);
        const int ty = sMapView.viewY + (int)((sTouchY - sMapView.y) / sMapView.ts);
        if (tx >= 0 && ty >= 0 && tx < sState->mapW && ty < sState->mapH && sState->canControl) {
            if (tx == sState->cursorX && ty == sState->cursorY)
                TapKey(PORT_KEY_A);
            else
                Port_S2MoveCursor(tx, ty);
        }
    }
    return -1;
}

uint16_t Port_SecondScreenKeys(void)
{
    const uint64_t now = Port_FrameCount();
    for (int b = 0; b < 16; b++) {
        if ((sTapKeys & (1 << b)) && now >= sTapUntil[b])
            sTapKeys &= (uint16_t)~(1 << b);
    }
    return sHeldKeys | sTapKeys;
}
