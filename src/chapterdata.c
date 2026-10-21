#include "global.h"
#include "chapterdata.h"
#include "agb_sram.h"
#include "bmsave.h"
#ifdef REGION_EU
#include "port/eu.h"
#endif

#include "src/data/chapter_settings.h"

#ifdef REGION_EU
// The port's chapter table is the US one; the EU messages are numbered
// differently, so take the chapter's message ids from the EU ROM's table.
// Rebuilt from constant data: kept out of save states (include/port/native.h).
#pragma clang section bss = "" data = ""
static struct ROMChapterData sEuChapters[0x80];
static u8 sEuChapterReady[0x80];
#pragma clang section bss = "fe8_bss" data = "fe8_data"

static const struct ROMChapterData* EuChapter(unsigned chIndex) {
    struct ROMChapterData* ch = &sEuChapters[chIndex];
    const u8* rom = gEuChapterDataTable + chIndex * EU_CHAPTER_DATA_SIZE;

    if (!sEuChapterReady[chIndex]) {
#define EU_U16(offset) (u16)(rom[offset] | (rom[(offset) + 1] << 8))
        *ch = gChapterDataTable[chIndex];
        ch->chapTitleTextId = EU_U16(0x70);
        ch->chapTitleTextIdInHectorStory = EU_U16(0x72);
        ch->divinationTextIdBeginning = EU_U16(0x76);
        ch->divinationTextIdInEliwoodStory = EU_U16(0x78);
        ch->divinationTextIdInHectorStory = EU_U16(0x7A);
        ch->divinationTextIdEnding = EU_U16(0x7C);
        ch->statusObjectiveTextId = EU_U16(0x88);
        ch->goalWindowTextId = EU_U16(0x8A);
#undef EU_U16
        sEuChapterReady[chIndex] = TRUE;
    }
    return ch;
}
#endif

const struct ROMChapterData* GetROMChapterStruct(unsigned chIndex) {
    if (chIndex == 0x7F)
        return gExtraMapInfo->chapter_info;

#ifdef REGION_EU
    if (chIndex < 0x80)
        return EuChapter(chIndex);
#endif
    return gChapterDataTable + chIndex;
}

const void* GetChapterMapPointer(unsigned chIndex) {
    if (chIndex != 0x7F)
        return CHAPTER_ASSET(GetROMChapterStruct(chIndex)->map.mainLayerId);

    ReadSramFast(GetExtraMapMapReadAddr(), gGenericBuffer, GetExtraMapMapSize());
    return gGenericBuffer;
}

const void* GetChapterMapChangesPointer(unsigned chIndex) {
    if (chIndex != 0x7F)
        return CHAPTER_ASSET(GetROMChapterStruct(chIndex)->map.changeLayerId);

    return gExtraMapInfo->map_change_info;
}

const struct ChapterEventGroup* GetChapterEventDataPointer(unsigned chIndex) {
    if (chIndex != 0x7F)
        return CHAPTER_ASSET(GetROMChapterStruct(chIndex)->mapEventDataId);

    return gExtraMapInfo->event_info;
}

const char * GetChapterTitleName(unsigned chIndex)
{
    if (chIndex != 0x7F)
        return GetStringFromIndex((int)(&GetROMChapterStruct(chIndex)->chapTitleTextId));

    return gExtraMapInfo->chapter_title;
}

u8 IsDifficultMode(void) {
    u8 difficultState = gPlaySt.chapterStateBits & PLAY_FLAG_HARD;
    return difficultState ? TRUE : FALSE;
}
