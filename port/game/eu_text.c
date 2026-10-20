// The European version's text code that has no US counterpart.
#include "global.h"
#include "fontgrp.h"
#include "bmitem.h"
#include "constants/items.h"
#include "sio.h"
#include "port/eu.h"

#ifdef REGION_EU

// The EU glyph tables are read from the ROM, whose glyphs link with 32-bit
// pointers: each table is converted once into native struct Glyph.
struct EuRomGlyph {
    u32 sjisNext;
    u8 sjisByte1;
    u8 width;
    u16 pad;
    u32 bitmap[16];
};

#define EU_GLYPH_TABLE_MAX (2 + EU_LANG_COUNT)

static struct {
    const void * rom;
    struct Glyph * table[0x100];
    struct Glyph glyphs[0x100];
} sEuGlyphTables[EU_GLYPH_TABLE_MAX];

// The native version of the ROM glyph table `rom_table` (`count` entries,
// at most 256). Shift-JIS links are not kept: the EU font never uses them.
struct Glyph ** EuGetGlyphTable(const void * rom_table, int count)
{
    int i, j;

    for (i = 0; i < EU_GLYPH_TABLE_MAX; i++) {
        if (sEuGlyphTables[i].rom == rom_table)
            return sEuGlyphTables[i].table;

        if (sEuGlyphTables[i].rom == NULL)
            break;
    }

    if (i == EU_GLYPH_TABLE_MAX)
        i = 0;

    sEuGlyphTables[i].rom = rom_table;

    for (j = 0; j < 0x100; j++) {
        const struct EuRomGlyph * src = j < count ? EU_ROM_PTR(rom_table, j) : NULL;
        struct Glyph * dst = &sEuGlyphTables[i].glyphs[j];

        if (src == NULL) {
            sEuGlyphTables[i].table[j] = NULL;
            continue;
        }

        dst->sjisNext = NULL;
        dst->sjisByte1 = src->sjisByte1;
        dst->width = src->width;
        memcpy(dst->bitmap, src->bitmap, sizeof(dst->bitmap));
        sEuGlyphTables[i].table[j] = dst;
    }

    return sEuGlyphTables[i].table;
}

// EU 0x0800a28c: the French indefinite article.
const char * EuGetFrenchArticle(bool feminine, bool capital)
{
    return EU_ROM_PTR(gEuStrPrefix_French, (feminine ? 2 : 0) + (capital ? 1 : 0));
}

// EU 0x0800a2b4: InsertPrefix with the French article when `prefix` is NULL.
void EuInsertFrenchArticle(char * str, const char * prefix, bool feminine, bool capital)
{
    const char * _prefix;
    u8 len_prefix;
    s16 i;

    if (prefix == NULL)
        _prefix = EuGetFrenchArticle(feminine, capital);
    else
        _prefix = prefix;

    len_prefix = strlen(_prefix);
    for (i = strlen(str); i >= 0; i--)
        str[i + len_prefix] = str[i];

    for (i = 0; i < len_prefix; i++)
        str[i] = _prefix[i];
}

// EU 0x080179b0: whether the item's name is feminine in the player's
// language. Byte 0x21 of the EU item data has one bit per language, in the
// order English, French, German, Spanish, Italian.
bool EuIsItemFeminine(int item)
{
    static const u8 gender_bit[EU_LANG_COUNT] = { 0, 2, 1, 3, 4 };
    int lang = GetLang();

    if (lang >= EU_LANG_COUNT)
        return FALSE;

    return (((const u8 *)&gItemData[ITEM_INDEX(item)])[0x21] >> gender_bit[lang]) & 1;
}

// EU 0x080163f8: the item's name with the French article ("une épée"...).
char * EuGetItemNameWithFrenchArticle(int item, bool capitalize)
{
    char * result;

    switch (ITEM_INDEX(item)) {
    case ITEM_1G:
    case ITEM_5G:
    case ITEM_10G:
    case ITEM_50G:
    case ITEM_100G:
    case ITEM_3000G:
    case ITEM_5000G:
    case ITEM_DEMONSTONE:
    case ITEM_ANIMA_AIRCALIBUR:
    case ITEM_150G:
    case ITEM_200G:
    case ITEM_UNK_CC:
        return GetItemName(item);
    }

    result = GetItemName(item);
    EuInsertFrenchArticle(result, NULL, EuIsItemFeminine(item), capitalize);
    return result;
}

// The EU tactician name entry grid (EU 0x080d941c, 64 bytes per key): it
// has the accented letters. The ROM entries hold 32-bit string pointers, so
// the grid is converted once into native struct TacticianTextConf.
#define EU_TACTICIAN_KEY_COUNT 0x51

const struct TacticianTextConf * EuGetTacticianTextConf(void)
{
    static struct TacticianTextConf conf[EU_TACTICIAN_KEY_COUNT];
    static bool loaded;
    int i, j;

    if (loaded)
        return conf;

    for (i = 0; i < EU_TACTICIAN_KEY_COUNT; i++) {
        const u8 * src = gEuTacticianTextConf + i * 0x40;

        for (j = 0; j < 0xC; j++)
            conf[i].str[j] = EU_ROM_PTR(src, j);

        conf[i].x = *(const u16 *)(src + 0x30);
        conf[i].y = *(const u16 *)(src + 0x32);
        conf[i].kind = src[0x34];
        for (j = 0; j < 4; j++)
            conf[i].adj_idx[j] = *(const s16 *)(src + 0x36 + j * 2);
        conf[i].action = src[0x3E];
    }

    loaded = TRUE;
    return conf;
}

#endif
