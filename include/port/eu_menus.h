#ifndef PORT_EU_MENUS_H
#define PORT_EU_MENUS_H

/* EU-only data and code: save, title, opening, world map, menus. Included by port/eu.h. */

/* Opening: health and safety screen (EU 0x080ccb28). */
extern const u32 gEuImgTable_HealthSafety[EU_LANG_COUNT];
extern const u32 gEuTsaTable_HealthSafety[EU_LANG_COUNT * 2];

/* Title screen: "Press Start" graphics (EU 0x080c6044). */
extern const u32 gEuGfxTable_TitlePressStart[EU_LANG_COUNT];

/* Save menu sprites (EU 0x080a960c) and main menu choices (EU 0x080a9848). */
extern const u32 gEuImgTable_SaveScreenSprits[EU_LANG_COUNT];
extern const u32 gEuImgTable_GameMainMenuObjs[EU_LANG_COUNT];

/* Difficulty menu sprites (EU 0x080ac9c8). */
extern const u32 gEuImgTable_DifficultyMenuObjs[EU_LANG_COUNT];

/* Sound room (EU 0x080afe48). The play status table is also read by the
 * chapter status screen (src/uichapterstatus.c). */
extern const u32 gEuImgTable_PlayStatusSprites[EU_LANG_COUNT];
extern const u32 gEuImgTable_SoundRoomUiElements[EU_LANG_COUNT];

/* Options screen sprites (EU 0x080b2348). */
extern const u32 gEuImgTable_ConfigUiSprites[EU_LANG_COUNT];

/* Ending: the "Fin" screen (EU 0x080b7dfc). */
extern const u32 gEuImgTable_FinScreen[EU_LANG_COUNT];
extern const u32 gEuTsaTable_FinScreen[EU_LANG_COUNT];

/* World map player interface (EU 0x080bf91c). */
extern const u32 gEuGfxTable_GMapPI_LevelNums[EU_LANG_COUNT];

/* World map "Skirmish" banner (EU 0x080c0fbc). */
extern const u32 gEuImgTable_WorldmapSkirmish[EU_LANG_COUNT];
extern const s16 gEuSkirmishXOffsets[EU_LANG_COUNT];

/* World map status screen (EU 0x080c15dc). */
extern const u32 gEuImgTable_WorldmapStatus[EU_LANG_COUNT];

/* World map nation highlights (EU 0x080c338c): per nation, an image per
 * language, the sprite and the position. */
extern const u32 gEuGfxSet_WmNationMap[];

/* World map minimap (EU 0x080c4698). */
extern const u32 gEuImgTable_WorldmapMinimap[EU_LANG_COUNT];

/* Opening subtitles (EU 0x080c5554): per language, a table of
 * {gfx, tsa, timer} words like gOpSubtitleGfxLut. */
extern const u32 gEuOpSubtitleGfxLutTable[EU_LANG_COUNT];

/* Guide (EU 0x080cf638). */
extern const u32 gEuImgTable_Guide_08B17864[EU_LANG_COUNT];
extern const u32 gEuImgTable_Guide_08B177C0[EU_LANG_COUNT];

/* Opening: Ephraim and Eirika's names (OpAnimInit, EU 0x080c7370). */
extern const u32 gEuImgTable_OpAnimEphEirikaName[EU_LANG_COUNT];

/* Language selection screen (port/game/eu_menus.c). */
extern const struct ProcCmd gProcScr_EuSelectLanguage[];
extern const char gEuStr_LangName0[]; /* the names, in the order listed */
extern const char gEuStr_LangName1[];
extern const char gEuStr_LangName2[];
extern const char gEuStr_LangName3[];
extern const char gEuStr_LangName4[];
extern const u32 gEuSprTable_LanguageFlag[EU_LANG_COUNT];
extern const u8 gEuImg_SelectLanguage[];
extern const u16 gEuPal_SelectLanguage[];
extern const u8 gEuTsa_SelectLanguage[];

#endif /* PORT_EU_MENUS_H */
