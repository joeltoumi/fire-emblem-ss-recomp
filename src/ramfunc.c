#include "global.h"

extern u8 gUnknown_03003750[];  // buffer to copy the code to

// pointers to the loaded functions
extern void (*gUnknown_03003740)(int, int, int, int);
extern void (*DecodeStringRAMFunc)(const char *, char *);
extern void (*gUnknown_03003130)(int, int, const u16 *, int);
extern void (*gUnknown_03004154)(int, int, const u16 *, int);
extern void (*gUnknown_03004960)(int, int, int);
extern void (*gUnknown_03003128)(void);

// arm.s symbols
extern const u8 ARMCodeToCopy_Start[];
extern const u8 DrawGlyph[];
extern const u8 DecodeString[];
extern const u8 PutOamHi[];
extern const u8 PutOamLo[];
extern const u8 MapFloodCoreStep[];
extern const u8 MapFloodCore[];
extern const u8 ARMCodeToCopy_End[];

#ifdef PLATFORM_PC
void Port_DrawGlyph(int, int, int, int);
void Port_DecodeString(const char *, char *);
void Port_PutOamHi(int, int, const u16 *, int);
void Port_PutOamLo(int, int, const u16 *, int);
void Port_MapFloodCoreStep(int, int, int);
void Port_MapFloodCore(void);

// The ARM routines are C functions natively (port/game/arm_routines.c).
void StoreRoutinesToIRAM(void)
{
    gUnknown_03003740 = Port_DrawGlyph;
    DecodeStringRAMFunc = Port_DecodeString;
    gUnknown_03003130 = Port_PutOamHi;
    gUnknown_03004154 = Port_PutOamLo;
    gUnknown_03004960 = Port_MapFloodCoreStep;
    gUnknown_03003128 = Port_MapFloodCore;
}
#else
void StoreRoutinesToIRAM(void)
{
    size_t armCodeSize = (ARMCodeToCopy_End - ARMCodeToCopy_Start);

    // Copy all of the ARM code into RAM
    CpuCopy16(ARMCodeToCopy_Start, gUnknown_03003750, armCodeSize + (armCodeSize >> 31));

    // Set pointers to each of the functions
    gUnknown_03003740 = (void *)(gUnknown_03003750 + (DrawGlyph    - ARMCodeToCopy_Start));
    DecodeStringRAMFunc = (void *)(gUnknown_03003750 + (DecodeString - ARMCodeToCopy_Start));
    gUnknown_03003130 = (void *)(gUnknown_03003750 + (PutOamHi - ARMCodeToCopy_Start));
    gUnknown_03004154 = (void *)(gUnknown_03003750 + (PutOamLo   - ARMCodeToCopy_Start));
    gUnknown_03004960 = (void *)(gUnknown_03003750 + (MapFloodCoreStep              - ARMCodeToCopy_Start));
    gUnknown_03003128 = (void *)(gUnknown_03003750 + (MapFloodCore    - ARMCodeToCopy_Start));
}
#endif

void DrawGlyphRam(int a, int b, int c, int d)
{
    gUnknown_03003740(a, b, c, d);
}

void CallARM_DecompText(const char *a, char *b)
{
    DecodeStringRAMFunc(a, b);
}

void CallARM_PushToSecondaryOAM(int a, int b, const u16 *c, int d)
{
    gUnknown_03003130(a, b, c, d);
}

void CallARM_PushToPrimaryOAM(int a, int b, const u16 *c, int d)
{
    gUnknown_03004154(a, b, c, d);
}

void CallARM_Func5(int a, int b, int c)
{
    gUnknown_03004960(a, b, c);
}

void CallARM_FillMovementMap()
{
    gUnknown_03003128();
}
