// fire-emblem-ss-recomp — C version of the m4a (MusicPlayer2000) sound
// engine's assembly half, src/m4a_1.s.
//
// Every function follows its assembly exactly: the same integer arithmetic
// (widths, signedness, shifts, truncations) and the same quirks, so the mixed
// PCM and the PSG register writes match the console's. The m4a structures keep
// their GBA layout natively (see include/gba/m4a_internal.h): their pointer
// fields are 32-bit addresses, valid as native pointers in the port.
//
// On the GBA, SoundMain copies the mixer (SoundMainRAM) to IWRAM and jumps
// there; here it is an ordinary function call. The DMA 1/2 transfers that feed
// the mixed buffer to the DirectSound FIFOs are reproduced by
// Port_SoundFifoBlock (called by port/src/apu.c).
#include "global.h"
#include "m4a.h"

u32 MidiKeyToFreq(struct WaveData *wav, u8 key, u8 fineAdjust);
extern void *const gMPlayJumpTableTemplate[];
extern const u8 gClockTable[];

#define VCOUNT_VBLANK   160
#define TOTAL_SCANLINES 228

// Channels: DirectSound and CGB channels share the first 0x40-byte layout
// (status, type, volumes, ADSR, key, gate time, priority, the track link and
// the pp/np chain), and the engine walks a track's mixed chain of both kinds
// through the SoundChannel view.
#define CHAN(addr)  M4A_GET(struct SoundChannel *, addr)
#define CGB(chan)   ((struct CgbChannel *)(chan))
#define TRACK(addr) M4A_GET(struct MusicPlayerTrack *, addr)

typedef void (*TrackFunc)(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
typedef void (*PlyNoteFunc)(u32, struct MusicPlayerInfo *, struct MusicPlayerTrack *);
typedef void (*IntpFunc)(void *);

// ---- small helpers ------------------------------------------------------------

u32 umul3232H32(u32 multiplier, u32 multiplicand)
{
    return (u32)(((u64)multiplier * multiplicand) >> 32);
}

// chk_adr_r2: reject addresses below EWRAM (the BIOS ROM), except the jump
// table template when it is inside the BIOS (never the case on a cartridge).
// The GBA reads such a byte (BIOS open bus) and then replaces it by 0; natively
// nothing is mapped there, so the read is skipped.
static bool32 AdrOk(uintptr_t adr)
{
    return (adr >> 25) != 0;
}

// ld_r3_tp_adr_i: the next byte of the track's command stream (checked).
static u32 ReadCmdByte(struct MusicPlayerTrack *track)
{
    u32 adr = track->cmdPtr;

    track->cmdPtr = adr + 1;
    if (!AdrOk(adr))
        return 0;
    return *(u8 *)(uintptr_t)adr;
}

// ld_r3_tp_adr_i_unchecked
static u32 ReadCmdByteUnchecked(struct MusicPlayerTrack *track)
{
    u32 adr = track->cmdPtr;

    track->cmdPtr = adr + 1;
    return *(u8 *)(uintptr_t)adr;
}

// ---- SoundMain / the PCM mixer ------------------------------------------------

static u32 VCountLine(void)
{
    u32 line = *(vu8 *)REG_ADDR_VCOUNT;

    if (line < VCOUNT_VBLANK)
        line += TOTAL_SCANLINES;
    return line;
}

// The "mul rX, r10/r11, sample; bic 0xFF0000; add rY, rX, rY, ror 8" sequence
// adds bits 8-15 of volume * sample to one output byte (the carry is lost).
static inline void MixSample(u8 *out, u32 er, u32 el, s32 sample)
{
    out[0] += (u8)(((s32)er * sample) >> 8);
    out[PCM_DMA_BUF_SIZE] += (u8)(((s32)el * sample) >> 8);
}

// SoundMainRAM: reverb (or clear) the buffer segment, then run each
// DirectSound channel's envelope and mix it in.
//   pcmOut: the segment of buffer A this V-blank fills (buffer B follows A)
//   dmaCounter: soundInfo->pcmDmaCounter as SoundMain read it
//   maxLines: the VCOUNT line where mixing must stop (0 = no limit)
static void SoundMainRAM(struct SoundInfo *soundInfo, s8 *pcmOut, u32 dmaCounter, s32 maxLines)
{
    const s32 samples = soundInfo->pcmSamplesPerVBlank;
    const u32 reverb = soundInfo->reverb;
    u32 divFreq;
    s32 chanCount;
    struct SoundChannel *chan;

    if (reverb != 0)
    {
        s8 *cur = pcmOut;
        const s8 *next = (dmaCounter == 2) ? soundInfo->pcmBuffer : pcmOut + samples;
        s32 i = samples;

        do
        {
            s32 v = cur[PCM_DMA_BUF_SIZE] + cur[0] + next[PCM_DMA_BUF_SIZE] + next[0];

            next++;
            v = (s32)(v * reverb) >> 9;
            if (v & 0x80)
                v += 1;
            cur[PCM_DMA_BUF_SIZE] = (s8)v;
            cur[0] = (s8)v;
            cur++;
        } while (--i > 0);
    }
    else
    {
        // Word stores: 1 if bit 2 of the count is set, 2 if bit 3 is, then
        // 4 at a time for count >> 4 (at least once).
        s32 bytes = 0;
        s32 loops = samples >> 4;

        if (samples & 4)
            bytes += 4;
        if (samples & 8)
            bytes += 8;
        bytes += 16 * (loops > 0 ? loops : 1);
        memset(pcmOut, 0, bytes);
        memset(pcmOut + PCM_DMA_BUF_SIZE, 0, bytes);
    }

    divFreq = soundInfo->divFreq;
    chanCount = soundInfo->maxChans;
    chan = soundInfo->chans;

    for (;;)
    {
        struct WaveData *wav = M4A_GET(struct WaveData *, chan->wav);
        u32 status;
        u32 env;
        s32 loopLen;
        const s8 *loopStart;
        u8 *out;
        s32 ct;
        const s8 *cp;
        u32 er, el;
        s32 k;

        if (maxLines != 0 && VCountLine() >= (u32)maxLines)
            break;

        status = chan->status;
        if (!(status & 0xC7))
            goto next_chan;

        if (status & 0x80)
        {
            if (status & 0x40)
            {
                chan->status = 0;
                goto next_chan;
            }
            status = 3;
            chan->status = status;
            chan->cp = M4A_ADDR(wav) + 0x10;
            chan->ct = wav->size;
            env = 0;
            chan->ev = 0;
            chan->fw = 0;
            if ((wav->status >> 8) & 0xC0)
            {
                status |= 0x10;
                chan->status = status;
            }
            goto attack;
        }

        env = chan->ev;
        if (status & 4)
        {
            // Pseudo echo: counts down echoLength.
            u32 len = chan->echoLength;

            chan->echoLength = len - 1;
            if (len > 1)
                goto envelope_done;
            chan->status = 0;
            goto next_chan;
        }
        else if (status & 0x40)
        {
            env = (env * chan->release) >> 8;
            if (env > chan->echoVolume)
                goto envelope_done;
        echo_start:
            env = chan->echoVolume;
            if (env == 0)
            {
                chan->status = 0;
                goto next_chan;
            }
            status |= 4;
            chan->status = status;
            goto envelope_done;
        }
        else
        {
            u32 phase = status & 3;

            if (phase == 2)
            {
                env = (env * chan->decay) >> 8;
                if (env > chan->sustain)
                    goto envelope_done;
                env = chan->sustain;
                if (env == 0)
                    goto echo_start;
                status--;
                chan->status = status;
                goto envelope_done;
            }
            if (phase != 3)
                goto envelope_done;
        attack:
            env += chan->attack;
            if (env >= 0xFF)
            {
                env = 0xFF;
                status--;
                chan->status = status;
            }
        }

    envelope_done:
        chan->ev = env;
        env = ((soundInfo->masterVolume + 1) * env) >> 4;
        chan->er = (chan->rightVolume * env) >> 8;
        chan->el = (chan->leftVolume * env) >> 8;

        loopLen = status & 0x10;
        loopStart = NULL;
        if (loopLen)
        {
            loopStart = wav->data + wav->loopStart;
            loopLen = wav->size - wav->loopStart;
        }

        out = (u8 *)pcmOut;
        ct = chan->ct;
        cp = M4A_GET(const s8 *, chan->cp);
        er = chan->er;
        el = chan->el;

        if (chan->type & TONEDATA_TYPE_FIX)
        {
            // Fixed frequency: one sample per output sample. (The assembly
            // mixes 4 at a time while ct stays above 4; same result.)
            for (k = 0; k < samples; k++)
            {
                s32 sample = *cp++;

                MixSample(out + k, er, el, sample);
                if (--ct == 0)
                {
                    if (loopLen != 0)
                    {
                        ct = loopLen;
                        cp = loopStart;
                    }
                    else
                    {
                        chan->status = 0;
                        goto next_chan;
                    }
                }
            }
            chan->ct = ct;
            chan->cp = M4A_ADDR(cp);
        }
        else
        {
            // Resampled with linear interpolation; fw is a 23-bit fraction.
            u32 fw = chan->fw;
            const u32 step = divFreq * chan->freq;
            s32 s0 = cp[0];
            s32 diff;

            cp++;
            diff = *cp - s0;

            for (k = 0; k < samples; k++)
            {
                s32 sample = s0 + ((s32)(fw * (u32)diff) >> 23);
                u32 advance;

                MixSample(out + k, er, el, sample);

                fw += step;
                advance = fw >> 23;
                if (advance == 0)
                    continue;
                fw &= ~0x3F800000u;
                ct = (s32)((u32)ct - advance);
                if (ct <= 0)
                {
                    s32 overshoot;

                    if (loopLen == 0)
                    {
                        // Stops without storing ct/cp/fw.
                        chan->status = 0;
                        goto next_chan;
                    }
                    overshoot = -ct;
                    for (;;)
                    {
                        ct = loopLen + ct;
                        if (ct > 0)
                            break;
                        overshoot -= loopLen;
                    }
                    cp = loopStart + overshoot;
                    s0 = *cp;
                }
                else if (advance - 1 == 0)
                {
                    s0 = s0 + diff;
                }
                else
                {
                    cp += advance - 1;
                    s0 = *cp;
                }
                cp++;
                diff = *cp - s0;
            }
            cp--;
            chan->fw = fw;
            chan->ct = ct;
            chan->cp = M4A_ADDR(cp);
        }

    next_chan:
        if (--chanCount <= 0)
            break;
        chan++;
    }

    soundInfo->ident = ID_NUMBER;
}

void SoundMain(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    s32 maxLines;
    s8 *pcmOut;
    u32 dmaCounter;

    if (soundInfo == NULL) // natively the BIOS word starts out null
        return;
    if (soundInfo->ident != ID_NUMBER)
        return;
    soundInfo->ident++;

    maxLines = soundInfo->maxLines;
    if (maxLines != 0)
        maxLines += VCountLine();

    if (soundInfo->func != 0)
        M4A_GET(IntpFunc, soundInfo->func)(M4A_GET(void *, soundInfo->intp));

    soundInfo->CgbSound();

    pcmOut = soundInfo->pcmBuffer;
    dmaCounter = soundInfo->pcmDmaCounter;
    if (dmaCounter > 1)
        pcmOut += (soundInfo->pcmDmaPeriod - (dmaCounter - 1)) * soundInfo->pcmSamplesPerVBlank;

    SoundMainRAM(soundInfo, pcmOut, dmaCounter, maxLines);
}

// Clear64byte (through gMPlayJumpTable[35]).
void SoundMainBTM(void *addr)
{
    memset(addr, 0, 64);
}

// Unlinks a channel from its track's channel chain (gMPlayJumpTable[34]).
void RealClearChain(void *x)
{
    struct SoundChannel *chan = x;
    struct MusicPlayerTrack *track = TRACK(chan->track);
    struct SoundChannel *next, *prev;

    if (track == NULL)
        return;
    next = CHAN(chan->np);
    prev = CHAN(chan->pp);
    if (prev != NULL)
        prev->np = M4A_ADDR(next);
    else
        track->chan = M4A_ADDR(next);
    if (next != NULL)
        next->pp = M4A_ADDR(prev);
    chan->track = 0;
}

// ---- sequence commands -----------------------------------------------------------

void ply_fine(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundChannel *chan = CHAN(track->chan);

    while (chan != NULL)
    {
        if (chan->status & 0xC7)
            chan->status |= 0x40;
        RealClearChain(chan);
        chan = CHAN(chan->np);
    }
    track->flags = 0;
}

void MPlayJumpTableCopy(void **mplayJumpTable)
{
    s32 i;

    for (i = 0; i < 0x24; i++)
    {
        void *func = AdrOk((uintptr_t)&gMPlayJumpTableTemplate[i]) ? gMPlayJumpTableTemplate[i] : NULL;

        mplayJumpTable[i] = func;
    }
}

void ply_goto(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    const u8 *p = M4A_GET(const u8 *, track->cmdPtr);

    if (!AdrOk((uintptr_t)p))
    {
        // The GBA would read the BIOS (garbage); nothing is mapped there.
        track->cmdPtr = 0;
        return;
    }
    track->cmdPtr = p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
}

void ply_patt(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 level = track->patternLevel;

    if (level >= 3)
    {
        ply_fine(mplayInfo, track);
        return;
    }
    track->patternStack[level] = track->cmdPtr + 4;
    track->patternLevel = level + 1;
    ply_goto(mplayInfo, track);
}

void ply_pend(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 level = track->patternLevel;

    if (level == 0)
        return;
    level--;
    track->patternLevel = level;
    track->cmdPtr = track->patternStack[level];
}

void ply_rept(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 start = track->cmdPtr;
    u32 count;
    u32 repN;

    if (*(u8 *)(uintptr_t)start == 0)
    {
        // Infinite loop.
        track->cmdPtr = start + 1;
        ply_goto(mplayInfo, track);
        return;
    }
    // Compared before its truncation to a byte (256 after 255).
    repN = track->repN + 1;
    track->repN = repN;
    count = ReadCmdByte(track);
    if (repN < count)
    {
        ply_goto(mplayInfo, track);
        return;
    }
    track->repN = 0;
    track->cmdPtr = start + 5;
}

void ply_prio(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    track->priority = ReadCmdByte(track);
}

void ply_tempo(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 tempo = ReadCmdByte(track) << 1;

    mplayInfo->tempoD = tempo;
    mplayInfo->tempoI = (tempo * mplayInfo->tempoU) >> 8;
}

void ply_keysh(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    track->keyShift = ReadCmdByte(track);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_voice(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 n = *(u8 *)(uintptr_t)track->cmdPtr;
    uintptr_t tone;

    track->cmdPtr++;
    tone = mplayInfo->tone + n * 12;
    // Three words, each "checked" against the address of the entry.
    if (AdrOk(tone))
        memcpy(&track->tone, (const void *)tone, sizeof(struct ToneData));
    else
        memset(&track->tone, 0, sizeof(struct ToneData));
}

void ply_vol(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    track->vol = ReadCmdByte(track);
    track->flags |= MPT_FLG_VOLCHG;
}

void ply_pan(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    track->pan = ReadCmdByte(track) - C_V;
    track->flags |= MPT_FLG_VOLCHG;
}

void ply_bend(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    track->bend = ReadCmdByte(track) - C_V;
    track->flags |= MPT_FLG_PITCHG;
}

void ply_bendr(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    track->bendRange = ReadCmdByte(track);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_lfodl(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    track->lfoDelay = ReadCmdByte(track);
}

void ply_modt(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 modT = ReadCmdByte(track);

    if (track->modT != modT)
    {
        track->modT = modT;
        track->flags |= MPT_FLG_VOLCHG | MPT_FLG_PITCHG;
    }
}

void ply_tune(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    track->tune = ReadCmdByte(track) - C_V;
    track->flags |= MPT_FLG_PITCHG;
}

// Writes a byte to a sound register: offset from SOUND1CNT_L, then value.
void ply_port(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 reg = *(u8 *)(uintptr_t)track->cmdPtr;
    u32 val;

    track->cmdPtr++;
    val = ReadCmdByte(track);
    *(vu8 *)(uintptr_t)(REG_ADDR_SOUND1CNT_L + reg) = val;
}

// clear_modM
static void ClearModMAsm(struct MusicPlayerTrack *track)
{
    track->modM = 0;
    track->lfoSpeedC = 0;
    track->flags |= (track->modT == 0) ? MPT_FLG_PITCHG : MPT_FLG_VOLCHG;
}

void ply_lfos(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 speed = ReadCmdByteUnchecked(track);

    track->lfoSpeed = speed;
    if (speed == 0)
        ClearModMAsm(track);
}

void ply_mod(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 mod = ReadCmdByteUnchecked(track);

    track->mod = mod;
    if (mod == 0)
        ClearModMAsm(track);
}

// ---- V-blank: DirectSound DMA restart ---------------------------------------------

// Port side of DMA 1/2: byte offset of the next 16-sample block they read from
// buffer A / B (the DMAs restart at the buffer start every pcmDmaPeriod
// V-blanks).
static u32 sPcmDmaCursor;

void Port_M4aDmaRestart(void)
{
    sPcmDmaCursor = 0;
}

int Port_SoundFifoBlock(const signed char **fifoA, const signed char **fifoB)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    if (soundInfo == NULL)
        return 0;
    // The real DMA would run on past the buffer if the restart never came;
    // stay inside it instead.
    if (sPcmDmaCursor + 16 > PCM_DMA_BUF_SIZE)
        sPcmDmaCursor = 0;
    *fifoA = soundInfo->pcmBuffer + sPcmDmaCursor;
    *fifoB = soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE + sPcmDmaCursor;
    sPcmDmaCursor += 16;
    return 1;
}

void m4aSoundVSync(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    s32 counter;

    if (soundInfo == NULL)
        return;
    // Only when ident is ID_NUMBER or ID_NUMBER + 1.
    if (soundInfo->ident - ID_NUMBER > 1)
        return;

    counter = soundInfo->pcmDmaCounter - 1;
    soundInfo->pcmDmaCounter = counter;
    if (counter > 0)
        return;
    soundInfo->pcmDmaCounter = soundInfo->pcmDmaPeriod;

    if (REG_DMA1CNT & (DMA_REPEAT << 16))
        REG_DMA1CNT = ((DMA_ENABLE | DMA_START_NOW | DMA_32BIT | DMA_SRC_INC | DMA_DEST_FIXED) << 16) | 4;
    if (REG_DMA2CNT & (DMA_REPEAT << 16))
        REG_DMA2CNT = ((DMA_ENABLE | DMA_START_NOW | DMA_32BIT | DMA_SRC_INC | DMA_DEST_FIXED) << 16) | 4;

    // Off, then on again in DirectSound FIFO mode: restarts at pcmBuffer.
    REG_DMA1CNT_H = DMA_32BIT;
    REG_DMA2CNT_H = DMA_32BIT;
    REG_DMA1CNT_H = DMA_ENABLE | DMA_START_SPECIAL | DMA_32BIT | DMA_REPEAT;
    REG_DMA2CNT_H = DMA_ENABLE | DMA_START_SPECIAL | DMA_32BIT | DMA_REPEAT;
    Port_M4aDmaRestart();
}

// ---- the sequencer ----------------------------------------------------------------

// Channel volumes from the note velocity, the pseudo-echo pan and the track's
// volumes (r4 = channel, r5 = track in the assembly).
static void ChnVolSetAsm(struct SoundChannel *chan, struct MusicPlayerTrack *track)
{
    s32 velocity = chan->ve;
    s32 rp = (s8)chan->rp;
    u32 v;

    v = (u32)((s32)(track->volMR * ((0x80 + rp) * velocity)) >> 14);
    if (v > 0xFF)
        v = 0xFF;
    chan->rightVolume = v;

    v = (u32)((s32)(track->volML * ((0x7F - rp) * velocity)) >> 14);
    if (v > 0xFF)
        v = 0xFF;
    chan->leftVolume = v;
}

void MPlayMain(struct MusicPlayerInfo *mplayInfo)
{
    struct SoundInfo *soundInfo;
    u32 tempoC;

    if (mplayInfo->ident != ID_NUMBER)
        return;
    mplayInfo->ident++;

    // The next player in the chain (MPlayOpen links them through func/intp).
    if (mplayInfo->func != 0)
        M4A_GET(IntpFunc, mplayInfo->func)(M4A_GET(void *, mplayInfo->intp));

    if ((s32)mplayInfo->status < 0)
        goto done;

    soundInfo = SOUND_INFO_PTR;
    FadeOutBody(mplayInfo);
    if ((s32)mplayInfo->status < 0)
        goto done;

    // 32-bit sums, compared before being stored as u16 (as the assembly).
    tempoC = mplayInfo->tempoC + mplayInfo->tempoI;
    for (;;)
    {
        s32 i;
        u32 bit;
        u32 active;
        struct MusicPlayerTrack *track;

        mplayInfo->tempoC = tempoC;
        if (tempoC < 150)
            break;

        i = mplayInfo->trackCount;
        track = TRACK(mplayInfo->tracks);
        bit = 1;
        active = 0;

        for (;;)
        {
            if (track->flags & MPT_FLG_EXIST)
            {
                struct SoundChannel *chan;

                active |= bit;

                for (chan = CHAN(track->chan); chan != NULL; chan = CHAN(chan->np))
                {
                    if (chan->status & 0xC7)
                    {
                        if (chan->gt != 0 && --chan->gt == 0)
                            chan->status |= 0x40;
                    }
                    else
                    {
                        ClearChain(chan);
                    }
                }

                if (track->flags & MPT_FLG_START)
                {
                    Clear64byte(track);
                    track->flags = MPT_FLG_EXIST;
                    track->bendRange = 2;
                    track->volX = 64;
                    track->lfoSpeed = 22;
                    track->tone.type = 1;
                }

                while (track->wait == 0)
                {
                    u32 cmd = *(u8 *)(uintptr_t)track->cmdPtr;

                    if (cmd < 0x80)
                    {
                        cmd = track->runningStatus;
                    }
                    else
                    {
                        track->cmdPtr++;
                        if (cmd >= 0xBD)
                            track->runningStatus = cmd;
                    }

                    if (cmd >= 0xCF)
                    {
                        M4A_GET(PlyNoteFunc, soundInfo->plynote)(cmd - 0xCF, mplayInfo, track);
                    }
                    else if (cmd > 0xB0)
                    {
                        void **table = M4A_GET(void **, soundInfo->MPlayJumpTable);

                        mplayInfo->cmd = cmd - 0xB1;
                        ((TrackFunc)table[cmd - 0xB1])(mplayInfo, track);
                        if (track->flags == 0)
                            goto next_track;
                    }
                    else
                    {
                        track->wait = gClockTable[cmd - 0x80];
                    }
                }

                track->wait--;

                if (track->lfoSpeed != 0 && track->mod != 0)
                {
                    if (track->lfoDelayC != 0)
                    {
                        track->lfoDelayC--;
                    }
                    else
                    {
                        u32 phase;
                        s32 wave;
                        s32 modM;

                        phase = (u8)(track->lfoSpeedC + track->lfoSpeed);
                        track->lfoSpeedC = phase;
                        // Triangle wave.
                        if ((u8)(phase - 0x40) < 0x80)
                            wave = 0x80 - phase;
                        else
                            wave = (s8)phase;
                        modM = (s32)(track->mod * wave) >> 6;
                        if ((u8)(track->modM ^ modM) != 0)
                        {
                            track->modM = modM;
                            track->flags |= (track->modT == 0) ? MPT_FLG_PITCHG : MPT_FLG_VOLCHG;
                        }
                    }
                }
            }
        next_track:
            if (--i <= 0)
                break;
            track++;
            bit <<= 1;
        }

        mplayInfo->clock++;
        if (active == 0)
        {
            mplayInfo->status = MUSICPLAYER_STATUS_PAUSE;
            goto done;
        }
        mplayInfo->status = active;
        tempoC = (u32)mplayInfo->tempoC - 150;
    }

    {
        s32 i = mplayInfo->trackCount;
        struct MusicPlayerTrack *track = TRACK(mplayInfo->tracks);

        for (;;)
        {
            if ((track->flags & MPT_FLG_EXIST) && (track->flags & (MPT_FLG_VOLCHG | MPT_FLG_PITCHG)))
            {
                struct SoundChannel *chan;

                TrkVolPitSet(mplayInfo, track);
                for (chan = CHAN(track->chan); chan != NULL; chan = CHAN(chan->np))
                {
                    u32 cgbType;

                    if (!(chan->status & 0xC7))
                    {
                        ClearChain(chan);
                        continue;
                    }
                    cgbType = chan->type & TONEDATA_TYPE_CGB;
                    if (track->flags & MPT_FLG_VOLCHG)
                    {
                        ChnVolSetAsm(chan, track);
                        if (cgbType != 0)
                            CGB(chan)->mo |= 1;
                    }
                    if (track->flags & MPT_FLG_PITCHG)
                    {
                        s32 key = chan->ky + (s8)track->keyM;

                        if (key < 0)
                            key = 0;
                        if (cgbType != 0)
                        {
                            CGB(chan)->fr = soundInfo->MidiKeyToCgbFreq(cgbType, key, track->pitM);
                            CGB(chan)->mo |= 2;
                        }
                        else
                        {
                            chan->freq = MidiKeyToFreq(M4A_GET(struct WaveData *, chan->wav), key, track->pitM);
                        }
                    }
                }
                track->flags &= 0xF0;
            }
            if (--i <= 0)
                break;
            track++;
        }
    }

done:
    mplayInfo->ident = ID_NUMBER;
}

void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundChannel *chan;

    if (!(track->flags & MPT_FLG_EXIST))
        return;
    for (chan = CHAN(track->chan); chan != NULL; chan = CHAN(chan->np))
    {
        if (chan->status != 0)
        {
            u32 cgbType = chan->type & TONEDATA_TYPE_CGB;

            if (cgbType != 0)
                SOUND_INFO_PTR->CgbOscOff(cgbType);
            chan->status = 0;
        }
        chan->track = 0;
    }
    track->chan = 0;
}

void ply_note(u32 noteLengthIndex, struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    const u8 *p;
    const struct ToneData *tone;
    u32 key;
    u32 priority;
    u32 cgbType;
    s32 rhythmPan = 0;
    struct SoundChannel *chan;
    s32 k;

    track->gateTime = gClockTable[noteLengthIndex];

    // Optional key, velocity and extra gate time.
    p = M4A_GET(const u8 *, track->cmdPtr);
    if (*p < 0x80)
    {
        track->key = *p;
        p++;
        if (*p < 0x80)
        {
            track->velocity = *p;
            p++;
            if (*p < 0x80)
            {
                track->gateTime += *p;
                p++;
            }
        }
        track->cmdPtr = M4A_ADDR(p);
    }

    if (track->tone.type & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL))
    {
        u32 index = track->key;

        // Key split: the tone's third word is the key split table.
        if (track->tone.type & TONEDATA_TYPE_SPL)
        {
            u32 table;

            memcpy(&table, &track->tone.attack, sizeof(table));
            index = *(const u8 *)(uintptr_t)(table + track->key);
        }
        tone = M4A_GET(const struct ToneData *, track->tone.wav + index * 12);
        if (tone->type & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL))
            return;
        key = track->key;
        if (track->tone.type & TONEDATA_TYPE_RHY)
        {
            if (tone->pan_sweep & 0x80)
                rhythmPan = ((s32)tone->pan_sweep - TONEDATA_P_S_PAN) << 1;
            key = tone->key;
        }
    }
    else
    {
        tone = &track->tone;
        key = track->key;
    }

    priority = mplayInfo->priority + track->priority;
    if (priority > 0xFF)
        priority = 0xFF;

    cgbType = tone->type & TONEDATA_TYPE_CGB;
    if (cgbType != 0)
    {
        if (soundInfo->cgbChans == NULL)
            return;
        chan = (struct SoundChannel *)((u8 *)soundInfo->cgbChans + ((cgbType - 1) << 6));
        if ((chan->status & 0xC7) && !(chan->status & 0x40))
        {
            if (chan->pr > priority)
                return;
            if (chan->pr == priority && chan->track < M4A_ADDR(track))
                return;
        }
    }
    else
    {
        // A free channel, else the lowest priority one (among the releasing
        // ones first), the highest track address on ties.
        u32 bestPriority = priority;
        u32 bestTrack = M4A_ADDR(track);
        bool32 releasing = FALSE;
        struct SoundChannel *best = NULL;
        s32 n = soundInfo->maxChans;

        chan = soundInfo->chans;
        do
        {
            u32 status = chan->status;

            if (!(status & 0xC7))
            {
                best = chan;
                break;
            }
            if (status & 0x40)
            {
                if (!releasing)
                {
                    releasing = TRUE;
                    bestPriority = chan->pr;
                    bestTrack = chan->track;
                    best = chan;
                    goto skip;
                }
            }
            else if (releasing)
            {
                goto skip;
            }

            if (chan->pr < bestPriority)
            {
                bestPriority = chan->pr;
                bestTrack = chan->track;
                best = chan;
            }
            else if (chan->pr == bestPriority)
            {
                if (chan->track > bestTrack)
                {
                    bestTrack = chan->track;
                    best = chan;
                }
                else if (chan->track == bestTrack)
                {
                    best = chan;
                }
            }
        skip:
            chan++;
        } while (--n > 0);

        chan = best;
        if (chan == NULL)
            return;
    }

    // Link the channel at the head of the track's chain.
    ClearChain(chan);
    chan->pp = 0;
    chan->np = track->chan;
    if (track->chan != 0)
        CHAN(track->chan)->pp = M4A_ADDR(chan);
    track->chan = M4A_ADDR(chan);
    chan->track = M4A_ADDR(track);

    track->lfoDelayC = track->lfoDelay;
    if (track->lfoDelay != 0)
        ClearModMAsm(track);
    TrkVolPitSet(mplayInfo, track);

    // One word: gateTime, key, velocity, runningStatus -> gt, mk, ve, pr.
    chan->gt = track->gateTime;
    chan->mk = track->key;
    chan->ve = track->velocity;
    chan->pr = priority;
    chan->ky = key;
    chan->rp = rhythmPan;
    chan->type = tone->type;
    chan->wav = tone->wav;
    chan->attack = tone->attack;
    chan->decay = tone->decay;
    chan->sustain = tone->sustain;
    chan->release = tone->release;
    chan->echoVolume = track->echoVolume;
    chan->echoLength = track->echoLength;
    ChnVolSetAsm(chan, track);

    k = chan->ky + (s8)track->keyM;
    if (k < 0)
        k = 0;

    if (cgbType != 0)
    {
        u32 sweep = tone->pan_sweep;

        CGB(chan)->le = tone->length;
        if ((sweep & 0x80) || !(sweep & 0x70))
            sweep = 8;
        CGB(chan)->sw = sweep;
        chan->freq = soundInfo->MidiKeyToCgbFreq(cgbType, k, track->pitM);
    }
    else
    {
        chan->freq = MidiKeyToFreq(M4A_GET(struct WaveData *, tone->wav), k, track->pitM);
    }

    chan->status = 0x80;
    track->flags &= 0xF0;
}

void ply_endtie(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 key = *(u8 *)(uintptr_t)track->cmdPtr;
    struct SoundChannel *chan;

    if (key < 0x80)
    {
        track->key = key;
        track->cmdPtr++;
    }
    else
    {
        key = track->key;
    }

    for (chan = CHAN(track->chan); chan != NULL; chan = CHAN(chan->np))
    {
        u32 status = chan->status;

        if ((status & 0x83) && !(status & 0x40) && chan->mk == key)
        {
            chan->status = status | 0x40;
            return;
        }
    }
}
