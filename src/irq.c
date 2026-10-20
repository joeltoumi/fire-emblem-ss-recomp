#include "global.h"
#include "functions.h"
#include "variables.h"

#define IRQ_COUNT 14
extern void *gIRQHandlers[IRQ_COUNT];
extern u32 IntrMain_Buffer[0x200];

void DummyIRQRoutine(void);

void StoreIRQToIRAM(void)
{
    int i;
    for(i = 0; i < IRQ_COUNT; i++)
        gIRQHandlers[i] = DummyIRQRoutine;

#ifdef PLATFORM_PC
    // The port's frame loop calls gIRQHandlers directly (port/src/frame.c).
#else
    CpuFastCopy(IrqMain, IntrMain_Buffer, sizeof IntrMain_Buffer);
    INTR_VECTOR = IntrMain_Buffer;
#endif
}

void DummyIRQRoutine(void)
{}

void SetIRQHandler(int index, void *irq)
{
    gIRQHandlers[index] = irq;
}
