#ifndef PORT_EU_H
#define PORT_EU_H

/* The European version (REGION_EU).
 *
 * Its code differs from the US one where it handles the five languages:
 * GetLang() returns the language (0 English, 1 German, 2 French, 3 Spanish,
 * 4 Italian) and per-language data is picked from tables in the EU ROM.
 * Those tables have no US counterpart, so they are declared here and placed
 * by port/data/eu_only/*.txt. The `REGION_EU` branches in src/ follow the EU
 * code (the EU address is given next to each). */

#include "global.h"
#include "fontgrp.h"

/* Messages per language in the message table (EU 0x08006a1c). */
#define EU_MSG_COUNT 0xD35

/* port/game/eu_language.c */
void EuSaveLanguage(int lang);
int EuLoadLanguage(void);

/* Entry `i` of a table of 32-bit ROM pointers. */
#define EU_ROM_PTR(table, i) ((void *)(uintptr_t)((const u32 *)(table))[i])

enum {
    EU_LANG_ENGLISH,
    EU_LANG_GERMAN,
    EU_LANG_FRENCH,
    EU_LANG_SPANISH,
    EU_LANG_ITALIAN,
    EU_LANG_COUNT,
};

/* EU-only data and code, by area. */
#include "port/eu_text.h"   /* fonts, messages, items, map UI, link arena */
#include "port/eu_battle.h" /* battle and map animations, status screens */
#include "port/eu_menus.h"  /* save/title/opening/world map/menus */

#endif /* PORT_EU_H */
