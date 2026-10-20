// The European version: code only the EU ROM has, for battle and map
// animations and the status screens (include/port/eu_battle.h).
#include "global.h"
#include "unitlistscreen.h"
#include "port/eu.h"

#ifdef REGION_EU

// The EU ROM keeps per-language lists of pointers (frame tables of the
// level-up backgrounds...) that the game code indexes as native pointer
// arrays: copies the `n` 32-bit ROM pointers of `rom_list` into `buf`.
void * EuLoadRomPtrList(void * buf, const void * rom_list, int n)
{
    void ** out = buf;
    int i;

    for (i = 0; i < n; i++)
        out[i] = EU_ROM_PTR(rom_list, i);

    return buf;
}

// EU 0x08095558: the x of a column of the unit list screen, moved by a
// per-language offset.
int EuGetUnitListColumnX(int page, int column)
{
    return gEuUnitListColumnOffsets[page][GetLang()][column] + gUnitListScreenFields[page][column].xColumn;
}

#endif
