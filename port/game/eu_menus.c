// The European version's language selection screen: code only the EU ROM has.
//
// The EU game control proc starts it at every boot, before the health and
// safety screen (EU 0x087a26f0: PROC_START_CHILD_BLOCKING after
// GameControl_CallEraseSaveEventWithKeyCombo). The cursor starts on the
// current language; A stores the choice in the save file.
#include "global.h"
#include "proc.h"
#include "hardware.h"
#include "fontgrp.h"
#include "bmlib.h"
#include "bm.h"
#include "icon.h"
#include "uiutils.h"
#include "functions.h"
#include "ctc.h"
#include "m4a.h"
#include "soundwrapper.h"
#include "bmsave.h"
#include "constants/songs.h"
#include "port/eu.h"

#ifdef REGION_EU

// EU 0x02000000 and 0x02000028.
static struct Text sLanguageTexts[EU_LANG_COUNT];
static s16 sSelectedLanguage;

// EU 0x080cfe50: the five language names, the selected one in white.
static void EuSelectLanguage_DrawNames(ProcPtr proc)
{
    // In the order the screen lists them, three tile rows apart.
    static const char * const names[EU_LANG_COUNT] = {
        gEuStr_LangName0, gEuStr_LangName1, gEuStr_LangName2,
        gEuStr_LangName3, gEuStr_LangName4,
    };
    int i;

    for (i = 0; i < EU_LANG_COUNT; i++)
        ClearText(&sLanguageTexts[i]);

    for (i = 0; i < EU_LANG_COUNT; i++)
        PutDrawText(&sLanguageTexts[i], gBG0TilemapBuffer + TILEMAP_INDEX(19, 3 + 3 * i),
            sSelectedLanguage != i, 0, 0, names[i]);
}

// EU 0x080cff40: the selected language's flag.
static void EuSelectLanguage_PutFlag(ProcPtr proc)
{
    PutSpriteExt(4, 0x30, 0x2C, EU_ROM_PTR(gEuSprTable_LanguageFlag, sSelectedLanguage), 0x2000);
}

// EU 0x08b9419c
static const struct ProcCmd sProcScr_EuSelectLanguageFlag[] = {
    PROC_REPEAT(EuSelectLanguage_PutFlag),
    PROC_END,
};

// EU 0x080cff70
static void EuSelectLanguage_Init(ProcPtr proc)
{
    int i;

    sSelectedLanguage = GetLang();

    SetupBackgrounds(NULL);
    BG_Fill(BG_GetMapBuffer(0), 0);
    BG_Fill(BG_GetMapBuffer(1), 0);
    BG_Fill(BG_GetMapBuffer(2), 0);

    ResetTextFont();
    ResetText();
    ResetIconGraphics_();
    LoadUiFrameGraphics();
    LoadObjUIGfx();

    gLCDControlBuffer.dispcnt.bg0_on = 1;
    gLCDControlBuffer.dispcnt.bg1_on = 1;
    gLCDControlBuffer.dispcnt.bg2_on = 1;
    gLCDControlBuffer.dispcnt.bg3_on = 1;
    gLCDControlBuffer.dispcnt.obj_on = 1;

    BG_Fill(gBG0TilemapBuffer, 0);
    BG_Fill(gBG1TilemapBuffer, 0);

    for (i = 0; i < EU_LANG_COUNT; i++)
        InitText(&sLanguageTexts[i], 14);

    EuSelectLanguage_DrawNames(proc);
    InitSystemTextFont();

    Decompress(gEuTsa_SelectLanguage, gGenericBuffer);
    CallARM_FillTileRect(gBG2TilemapBuffer, gGenericBuffer, 0x1000);
    Decompress(gEuImg_SelectLanguage, (void *)0x06011000);
    ApplyPalette(gEuPal_SelectLanguage, 0x12);

    StartMuralBackgroundExt(proc, NULL, 0, 0, 0);
    BG_EnableSyncByMask(BG0_SYNC_BIT | BG1_SYNC_BIT | BG2_SYNC_BIT | BG3_SYNC_BIT);

    Proc_Start(sProcScr_EuSelectLanguageFlag, proc);
}

// EU 0x080d0084
static void EuSelectLanguage_Loop(ProcPtr proc)
{
    if (gKeyStatusPtr->newKeys & A_BUTTON) {
        PlaySoundEffect(SONG_SE_SYS_WINDOW_SELECT1);
        EuSaveLanguage(sSelectedLanguage);
        Proc_Break(proc);
        return;
    }

    if (gKeyStatusPtr->repeatedKeys & (DPAD_UP | DPAD_DOWN)) {
        if (gKeyStatusPtr->repeatedKeys & DPAD_UP) {
            if (sSelectedLanguage != 0)
                sSelectedLanguage--;
        } else {
            if (sSelectedLanguage < EU_LANG_COUNT - 1)
                sSelectedLanguage++;
        }

        EuSelectLanguage_DrawNames(proc);
        PlaySoundEffect(SONG_SE_SYS_CURSOR_UD1);
    }

    DisplayUiHand(0x90, sSelectedLanguage * 24 + 24);
}

// EU 0x08b941ac
const struct ProcCmd gProcScr_EuSelectLanguage[] = {
    PROC_NAME("SEL LANG"),
    PROC_CALL(EuSelectLanguage_Init),
    PROC_REPEAT(EuSelectLanguage_Loop),
    PROC_CALL(StartMidFadeToBlack),
    PROC_REPEAT(WaitForFade),
    PROC_CALL(EndMuralBackground),
    PROC_SLEEP(0),
    PROC_END,
};

#endif
