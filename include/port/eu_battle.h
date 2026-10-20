#ifndef PORT_EU_BATTLE_H
#define PORT_EU_BATTLE_H

/* EU-only data and code: battle and map animations, status screens. Included by port/eu.h. */

/* port/game/eu_battle.c */
void * EuLoadRomPtrList(void * buf, const void * rom_list, int n);

/* Battle: HIT/DMG/CRT labels (NewEkrGauge, EU 0x080511e4). */
extern const u32 gEuImgTable_EfxSideHitDmgCrit[EU_LANG_COUNT];
/* Battle: "No damage"/"Miss" scripts, two per language (NewEfxDamageMojiEffectOBJ, EU 0x0806c810). */
extern const u32 gEuAnimScrTable_NoDamageMiss[EU_LANG_COUNT * 2];
/* Battle: level-up backgrounds, per language a list of 11 TSA / images
 * (NewEfxlvupbg, EU 0x08074a38) and of 6 TSA and an image (NewEfxLvupBG2, EU 0x08074af8). */
extern const u32 gEuTsaLutTable_EfxLvupBG[EU_LANG_COUNT];
extern const u32 gEuImgLutTable_EfxLvupBG[EU_LANG_COUNT];
extern const u32 gEuTsaLutTable_EfxLvupBG2[EU_LANG_COUNT];
extern const u32 gEuImgTable_EfxLvupBG2[EU_LANG_COUNT];
/* Map animation: "EXP" label of the experience bar (ProcMAExpBar_OnDraw, EU 0x0807c208). */
extern const u32 gEuImgTable_EkrExpBar[EU_LANG_COUNT];

/* Map animation: "Miss"/"No damage" labels (MapAnim_BeginMISSAnim, EU 0x0807cd08,
 * and MapAnim_BeginNODAMAGEAnim, EU 0x0807cdd4). */
struct EuMapAnimMojiPos {
    /* 00 */ u8 min_x2;  /* below this (2 * screen tile x), the label is put at x_left */
    /* 01 */ u8 max_x2;  /* above it, at x_right */
    /* 02 */ s16 x_left;
    /* 04 */ s16 x_right;
    /* 06 */ s16 unused;
};
extern const struct EuMapAnimMojiPos gEuMapAnimMojiPos_Miss[EU_LANG_COUNT];
extern const struct EuMapAnimMojiPos gEuMapAnimMojiPos_NoDamage[EU_LANG_COUNT];
extern const u32 gEuImgTable_MapAnimMISS[EU_LANG_COUNT];
extern const u32 gEuObjTable_MapAnimMISS[EU_LANG_COUNT];
extern const u32 gEuImgTable_MapAnimNODAMAGE[EU_LANG_COUNT];
extern const u32 gEuObjTable_MapAnimNODAMAGE[EU_LANG_COUNT];
/* Map animation: "Level up" label (ManimLevelUp_StartLevelUpText, EU 0x0807f750). */
struct EuManimLevelUpTextPos {
    /* 00 */ u8 min_x2;  /* below this (2 * screen tile x), the label is put at x_left */
    /* 01 */ u8 max_x2;  /* above it, at x_right */
    /* 02 */ u8 x_left;
    /* 03 */ u8 x_right;
};
extern const struct EuManimLevelUpTextPos gEuManimLevelUpTextPos[EU_LANG_COUNT];
extern const u32 gEuImgTable_ManimLevelUpText[EU_LANG_COUNT];
extern const u32 gEuApTable_ManimLevelUpText[EU_LANG_COUNT];
/* Event map animation: "Congratulations"/"Map clear" labels (DisplayEventMapAnim, EU 0x08085f94). */
extern const u32 gEuImgTable_Congratulations[EU_LANG_COUNT];
extern const u32 gEuImgTable_MapClear[EU_LANG_COUNT];
/* Stat screen: page names (DisplayPageNameSprite, EU 0x08088228). */
struct EuPageNameSprite {
    /* 00 */ u32 sprite;  /* ROM pointer to the sprite (u16 OAM data) */
    /* 04 */ s16 x;
    /* 06 */ s16 y;
};
extern const struct EuPageNameSprite gEuPageNameSprites[EU_LANG_COUNT][3];
extern const u16 gEuPageNameChrOffsetLut[3];
/* Stat screen: object graphics and equipment labels (StatScreen_InitDisplay, EU 0x08088834). */
extern const u32 gEuImgTable_StatscreenObjs[EU_LANG_COUNT];
extern const u32 gEuImgTable_StatscreenEquipmentText[EU_LANG_COUNT];
/* Chapter title images: per language a table like chap_title_data, with
 * entries of 3 ROM pointers (the image first) (PutChapterTitleGfx, EU 0x080899b4). */
extern const u32 gEuChapTitleTables[EU_LANG_COUNT];
/* Help box graphics: 5 parts, each with an entry per language
 * (LoadHelpBoxGfx, EU 0x08089b9c, sub_80898C4, EU 0x08089c84, InitBoxDialogue, EU 0x0808a9e8). */
extern const u32 gEuGfxTable_HelpTextBox[5 * EU_LANG_COUNT];
/* Player interface: status names of the unit window (PutUnitMapUiStatus, EU 0x0808c7b8). */
extern const u32 gEuGfxTable_StatusText[EU_LANG_COUNT];
/* Player interface: tiles of the unit/terrain windows (InitPlayerPhaseInterface, EU 0x0808d3f8). */
extern const u32 gEuGfxTable_PlayerInterfaceFontTiles[EU_LANG_COUNT];
/* MenuButtonDisp_Init (EU 0x0808dd4c) also reads gEuImgTable_PrepHelpButtonSprites
 * (port/eu_text.h), and ChapterStatus_Init (EU 0x0808e40c) gEuImgTable_PlayStatusSprites
 * (port/eu_menus.h). */
/* Chapter status screen: sprite labels (StatusScreenSpriteDraw_Init, EU 0x0808ecb8). */
extern const u32 gEuImgTable_StatusScreenLabelSprites[EU_LANG_COUNT];
/* Chapter status screen: the play count label, wider than the US one
 * (StatusScreenSpriteDraw_Loop, EU 0x0808ed24). */
extern const u16 gEuSprite_ChapterStatus_PlayCountLabel[];
/* Unit list screen: page banners (sub_8090D80, EU 0x080912e8). */
extern const u32 gEuImgTable_UnitListBanners[EU_LANG_COUNT];
extern const u32 gEuImgTable_UnitListBanner_Animation[EU_LANG_COUNT];
/* Unit list screen: per page, language and column, an offset added to the
 * column's x (EuGetUnitListColumnX, EU 0x08095558). */
extern const s8 gEuUnitListColumnOffsets[10][EU_LANG_COUNT][9];
int EuGetUnitListColumnX(int page, int column);
/* Preparation screens: "Chapter"/"Information" banners (sub_8095C50, EU 0x080961f8). */
extern const u32 gEuImgTable_08A1AC88[EU_LANG_COUNT];
/* Preparations menu: "Preparations" title and "Menu"/"Start" buttons
 * (AtMenu_Reinitialize, EU 0x08096234; PrepUnit_InitGfx, EU 0x0809ae5c). */
extern const u32 gEuImgTable_08A1A4C8[EU_LANG_COUNT];
extern const u32 gEuImgTable_08A1D510[EU_LANG_COUNT];
/* Preparation screens: "Funds" label (DrawFundsSprite_Init, EU 0x08098a00). */
extern const u32 gEuImgTable_PrepFunds[EU_LANG_COUNT];
/* Support screen: menu banner (DrawSupportBannerSprites_Init, EU 0x080a1960; also read by
 * SupportSubScreen_SetupGraphics, EU 0x080a2b1c) and screen banner (SupportScreen_SetupGraphics, EU 0x080a1b0c). */
extern const u32 gEuGfxTable_SupportMenu[EU_LANG_COUNT];
extern const u32 gEuGfxTable_SupportScreenBanner[EU_LANG_COUNT];

#endif /* PORT_EU_BATTLE_H */
