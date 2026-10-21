#ifndef PORT_EU_TEXT_H
#define PORT_EU_TEXT_H

/* EU-only data and code: fonts, messages, items, map UI, link arena. Included by port/eu.h. */

/* Fonts. The EU font has the accented letters of the European languages, and
 * no Shift-JIS path: only a font whose lang is 6 (GetLang() never returns it)
 * looks glyphs up by Shift-JIS byte pair (Text_DrawString, EU 0x08003fc4). */
#define EU_FONT_LANG_SJIS 6

/* The ROM's glyph tables (256 entries, indexed by character). */
extern const u32 gEuTextGlyphs_System[0x100];
extern const u32 gEuTextGlyphs_Talk[0x100];
/* Special characters (numbers, HP/LV labels...): one table per language (AddSpecialChar, EU 0x08004a64). */
extern const u32 gEuTextGlyphsTable_Special[EU_LANG_COUNT];
#define EU_SPECIAL_GLYPH_COUNT 64

/* The chapter table as the EU ROM has it (GetROMChapterStruct, EU 0x080349d8):
 * 0x94 bytes per chapter, laid out like struct ROMChapterData. Its message
 * ids (chapter titles, goals) differ from the US ones the port's table holds. */
extern const u8 gEuChapterDataTable[];
#define EU_CHAPTER_DATA_SIZE 0x94

/* French indefinite articles: "un ", "Un ", "une ", "Une " (EU 0x0800a28c). */
extern const u32 gEuStrPrefix_French[4];

/* Map UI object graphics, per language (LoadObjUIGfx, EU 0x08015650). */
extern const u32 gEuImgTable_MiscUiGraphics[EU_LANG_COUNT];

/* Phase change names, per language (PhaseIntro_InitGraphics, EU 0x0801f588). */
extern const u32 gEuImgTable_PhaseChangePlayer[EU_LANG_COUNT];
extern const u32 gEuImgTable_PhaseChangeEnemy[EU_LANG_COUNT];
extern const u32 gEuImgTable_PhaseChangeOther[EU_LANG_COUNT];

/* Game over: "Game Over" image per language, and per language its x, y and
 * TSA (3 words) on BG0 (GameOverScreen_Init, EU 0x08021600). */
extern const u32 gEuImgTable_GameOverText[EU_LANG_COUNT];
extern const u32 gEuGameOverTextLayout[EU_LANG_COUNT * 3];

/* Preparations map: help button labels, per language (PrepHelpPrompt_Init, EU 0x08033904). */
extern const u32 gEuImgTable_PrepHelpButtonSprites[EU_LANG_COUNT];

/* Dungeon record: title image and TSA per language (SetupDungeonRecordUi,
 * EU 0x08038600), and the labels, 24 bytes each: message id, then x and y
 * per language at 4 + lang * 4 (DrawDungeonRecordUiLabels, EU 0x0803881c). */
extern const u32 gEuImgTable_DungeonRecordTitle[EU_LANG_COUNT];
extern const u32 gEuTsaTable_DungeonRecordTitle[EU_LANG_COUNT];
extern const u8 gEuDungeonRecordLabels[];

/* Link arena: tactician name entry grid, 0x51 keys of 64 bytes (GetTacticianTextConf, EU 0x080447b8). */
extern const u8 gEuTacticianTextConf[];

/* Link arena graphics, per language. */
extern const u32 gEuImgTable_TacticianSelObj[EU_LANG_COUNT]; /* Link arena: tactician/team list objects (SioTeamList_SetupGfx, EU 0x08043b48) */
extern const u32 gEuImgTable_085AC604[EU_LANG_COUNT]; /* Link arena: team list objects (SioTeamList_SetupGfx, EU 0x08043b48) */
extern const u32 gEuImgTable_LinkArenaPlayerBanners[EU_LANG_COUNT]; /* Link arena: player banners (SioPostBattle_Init, EU 0x080457e4) */
extern const u32 gEuImgTable_LinkArenaPlacementRanks[EU_LANG_COUNT]; /* Link arena: placement ranks (SioPostBattle_Init, EU 0x080457e4) */
extern const u32 gEuImgTable_LinkArenaRankIcons[EU_LANG_COUNT]; /* Link arena: rank icons (sub_8046838, EU 0x08046a24) */
extern const u32 gEuImgTable_LinkArenaMenu[EU_LANG_COUNT]; /* Link arena: main menu objects (SioMenu_LoadGraphics, EU 0x08047da0) */
extern const u32 gEuImgTable_085AD80C[EU_LANG_COUNT]; /* Link arena: points boxes (LAPointsBox_LoadBoxes, EU 0x08048c04) */
extern const u32 gEuImgTable_085AAE0C[EU_LANG_COUNT]; /* Link arena: title banners (LATitleBanner_Init, EU 0x0804c66c) */
extern const u32 gEuImgTable_085AE7EC[EU_LANG_COUNT]; /* Link arena: phase intro graphics (LAPhaseIntro_Init, EU 0x0804d554) */
/* Link arena phase intro names (LAPhaseIntro_Init, EU 0x0804d554): per
 * language 20 bytes, the four names by faction then their tile offset (u16). */
extern const u8 gEuLAPhaseIntroNames[];

/* port/game/eu_text.c */
struct Glyph ** EuGetGlyphTable(const void * rom_table, int count);
const char * EuGetFrenchArticle(bool feminine, bool capital);
void EuInsertFrenchArticle(char * str, const char * prefix, bool feminine, bool capital);
char * EuGetItemNameWithFrenchArticle(int item, bool capitalize);
bool EuIsItemFeminine(int item);
const struct TacticianTextConf * EuGetTacticianTextConf(void);

#endif /* PORT_EU_TEXT_H */
