// The European version's language: code only the EU ROM has.
//
// GetLang() (src/fontgrp.c) returns the current language, which the EU
// version keeps in the save file's global header (bits 1-5 of byte 0x0F).
#include "global.h"
#include "fontgrp.h"
#include "bmsave.h"
#include "port/eu.h"

#ifdef REGION_EU

// EU 0x080a4968: switches to `lang` and stores it in the save file.
void EuSaveLanguage(int lang)
{
    struct GlobalSaveInfo info;

    if (ReadGlobalSaveInfo(&info)) {
        u8 *flags = (u8 *)&info + 0x0F;
        if (((*flags >> 1) & 0x1F) != lang) {
            *flags = (*flags & ~0x3E) | ((lang & 0x1F) << 1);
            WriteGlobalSaveInfo(&info);
        }
    }
    SetLang(lang);
}

// EU 0x080a49b0: the language stored in the save file (0 if there is no
// save yet), which becomes the current one.
int EuLoadLanguage(void)
{
    struct GlobalSaveInfo info;
    int lang;

    if (!ReadGlobalSaveInfo(&info))
        return 0;
    lang = (((u8 *)&info)[0x0F] >> 1) & 0x1F;
    SetLang(lang);
    return lang;
}

#endif

// EU 0x08006a1c: the message table holds every language in turn.
int Port_LocalizeMsg(int index)
{
#ifdef REGION_EU
    if (index != 0)
        index += GetLang() * EU_MSG_COUNT;
#endif
    return index;
}
