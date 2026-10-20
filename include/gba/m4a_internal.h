#ifndef GUARD_M4A_INTERNAL_H
#define GUARD_M4A_INTERNAL_H

#include "gba/gba.h"

// Pointer fields of the engine's structures.
//
// The sound data in ROM (song headers, voicegroups, the song and music player
// tables) stores 32-bit GBA addresses, and the engine copies or clears its RAM
// structures with fixed GBA sizes (Clear64byte, the 12-byte ToneData copy in
// ply_voice, the 0x40-byte channel stride shared by DirectSound and CGB
// channels...). So natively every m4a structure keeps its exact GBA layout and
// its pointers are 32-bit addresses (u32). That works because the port maps
// the GBA memory regions at their hardware addresses and loads the game below
// 4 GiB (see port/platform.h). On the GBA these macros change nothing.
//   M4A_PTR(type)       declares such a field
//   M4A_GET(type, x)    reads one as a native pointer
//   M4A_SET(p)          the value to store into one
//   M4A_ADDR(p)         a pointer as a u32 (fields declared u32 on the GBA too)
#ifdef PLATFORM_PC
#define M4A_PTR(type)    u32
#define M4A_GET(type, x) ((type)(uintptr_t)(x))
#define M4A_SET(p)       ((u32)(uintptr_t)(p))
#define M4A_ADDR(p)      ((u32)(uintptr_t)(p))
#else
#define M4A_PTR(type)    type
#define M4A_GET(type, x) (x)
#define M4A_SET(p)       (p)
#define M4A_ADDR(p)      ((u32)(p))
#endif

// ASCII encoding of 'Smsh' in reverse
// This is presumably short for SMASH, the developer of MKS4AGB.
#define ID_NUMBER 0x68736D53

#define C_V 0x40 // center value for PAN, BEND, and TUNE

#define SOUND_MODE_REVERB_VAL   0x0000007F
#define SOUND_MODE_REVERB_SET   0x00000080
#define SOUND_MODE_MAXCHN       0x00000F00
#define SOUND_MODE_MAXCHN_SHIFT 8
#define SOUND_MODE_MASVOL       0x0000F000
#define SOUND_MODE_MASVOL_SHIFT 12
#define SOUND_MODE_FREQ_05734   0x00010000
#define SOUND_MODE_FREQ_07884   0x00020000
#define SOUND_MODE_FREQ_10512   0x00030000
#define SOUND_MODE_FREQ_13379   0x00040000
#define SOUND_MODE_FREQ_15768   0x00050000
#define SOUND_MODE_FREQ_18157   0x00060000
#define SOUND_MODE_FREQ_21024   0x00070000
#define SOUND_MODE_FREQ_26758   0x00080000
#define SOUND_MODE_FREQ_31536   0x00090000
#define SOUND_MODE_FREQ_36314   0x000A0000
#define SOUND_MODE_FREQ_40137   0x000B0000
#define SOUND_MODE_FREQ_42048   0x000C0000
#define SOUND_MODE_FREQ         0x000F0000
#define SOUND_MODE_FREQ_SHIFT   16
#define SOUND_MODE_DA_BIT_9     0x00800000
#define SOUND_MODE_DA_BIT_8     0x00900000
#define SOUND_MODE_DA_BIT_7     0x00A00000
#define SOUND_MODE_DA_BIT_6     0x00B00000
#define SOUND_MODE_DA_BIT       0x00B00000
#define SOUND_MODE_DA_BIT_SHIFT 20

struct WaveData
{
    u16 type;
    u16 status;
    u32 freq;
    u32 loopStart;
    u32 size; // number of samples
    s8 data[1]; // samples
};

#define TONEDATA_TYPE_CGB    0x07
#define TONEDATA_TYPE_FIX    0x08
#define TONEDATA_TYPE_SPL    0x40 // key split
#define TONEDATA_TYPE_RHY    0x80 // rhythm

#define TONEDATA_P_S_PAN    0xc0
#define TONEDATA_P_S_PAM    TONEDATA_P_S_PAN

struct ToneData
{
    u8 type;
    u8 key;
    u8 length; // sound length (compatible sound)
    u8 pan_sweep; // pan or sweep (compatible sound ch. 1)
    M4A_PTR(struct WaveData *) wav;
    u8 attack;
    u8 decay;
    u8 sustain;
    u8 release;
};

struct CgbChannel
{
    u8 sf; // 0x0
    u8 ty; // 0x1
    u8 rightVolume; // 0x2
    u8 leftVolume; // 0x3
    u8 at; // 0x4
    u8 de; // 0x5
    u8 su; // 0x6
    u8 re; // 0x7
    u8 ky; // 0x8
    u8 ev; // 0x9
    u8 eg; // 0xA
    u8 ec; // 0xB
    u8 echoVolume; // 0xC
    u8 echoLength; // 0xD
    u8 d1; // 0xE
    u8 d2; // 0xF
    u8 gt; // 0x10
    u8 mk; // 0x11
    u8 ve; // 0x12
    u8 pr; // 0x13
    u8 rp; // 0x14
    u8 d3[3]; // 0x15, 0x16, 0x17
    u8 d5; // 0x18
    u8 sg; // 0x19
    u8 n4; // 0x1A
    u8 pan; // 0x1B
    u8 panMask; // 0x1C
    u8 mo; // 0x1D
    u8 le; // 0x1E
    u8 sw; // 0x1F
    u32 fr; // 0x20
    M4A_PTR(u32 *) wp;
    u32 cp;
    u32 tp;
    u32 pp;
    u32 np;
    u8 d4[8];
};

struct MusicPlayerTrack;

struct SoundChannel
{
    u8 status;
    u8 type;
    u8 rightVolume;
    u8 leftVolume;
    u8 attack;
    u8 decay;
    u8 sustain;
    u8 release;
    u8 ky;
    u8 ev;
    u8 er;
    u8 el;
    u8 echoVolume;
    u8 echoLength;
    u8 d1;
    u8 d2;
    u8 gt;
    u8 mk;
    u8 ve;
    u8 pr;
    u8 rp;
    u8 d3[3];
    u32 ct;
    u32 fw;
    u32 freq;
    M4A_PTR(struct WaveData *) wav;
    u32 cp;
    M4A_PTR(struct MusicPlayerTrack *) track;
    u32 pp;
    u32 np;
    u32 d4;
    u16 xpi;
    u16 xpc;
};

#define MAX_DIRECTSOUND_CHANNELS 12

#define PCM_DMA_BUF_SIZE 1584 // size of Direct Sound buffer

struct SoundInfo
{
    // This field is normally equal to ID_NUMBER but it is set to other
    // values during sensitive operations for locking purposes.
    // This field should be volatile but isn't. This could potentially cause
    // race conditions.
    u32 ident;

    vu8 pcmDmaCounter;

    // Direct Sound
    u8 reverb;
    u8 maxChans;
    u8 masterVolume;
    u8 freq;

    u8 mode;
    u8 c15;
    u8 pcmDmaPeriod; // number of V-blanks per PCM DMA
    u8 maxLines;
    u8 gap[3];
    s32 pcmSamplesPerVBlank;
    s32 pcmFreq;
    s32 divFreq;
    struct CgbChannel *cgbChans;
    u32 func;
    u32 intp;
    void (*CgbSound)(void);
    void (*CgbOscOff)(u8);
    u32 (*MidiKeyToCgbFreq)(u8, u8, u8);
    u32 MPlayJumpTable;
    u32 plynote;
    u32 ExtVolPit;
    u8 gap2[16];
    struct SoundChannel chans[MAX_DIRECTSOUND_CHANNELS];
    s8 pcmBuffer[PCM_DMA_BUF_SIZE * 2];
};

struct SongHeader
{
    u8 trackCount;
    u8 blockCount;
    u8 priority;
    u8 reverb;
    M4A_PTR(struct ToneData *) tone;
    M4A_PTR(u8 *) part[1];
};

struct PokemonCrySong
{
    u8 trackCount;
    u8 blockCount;
    u8 priority;
    u8 reverb;
    M4A_PTR(struct ToneData *) tone;
    M4A_PTR(u8 *) part[2];
    u8 gap;
    u8 part0; // 0x11
    u8 tuneValue; // 0x12
    u8 gotoCmd; // 0x13
    u32 gotoTarget; // 0x14
    u8 part1; // 0x18
    u8 tuneValue2; // 0x19
    u8 cont[2]; // 0x1A
    u8 volCmd; // 0x1C
    u8 volumeValue; // 0x1D
    u8 unkCmd0D[2]; // 0x1E
    u32 unkCmd0DParam; // 0x20
    u8 xreleCmd[2]; // 0x24
    u8 releaseValue; // 0x26
    u8 panCmd;
    u8 panValue; // 0x28
    u8 tieCmd; // 0x29
    u8 tieKeyValue; // 0x2A
    u8 tieVelocityValue; // 0x2B
    u8 unkCmd0C[2]; // 0x2C
    u16 unkCmd0CParam; // 0x2E
    u8 end[2]; // 0x30
};

#define MPT_FLG_VOLSET 0x01
#define MPT_FLG_VOLCHG 0x03
#define MPT_FLG_PITSET 0x04
#define MPT_FLG_PITCHG 0x0C
#define MPT_FLG_START  0x40
#define MPT_FLG_EXIST  0x80

struct MusicPlayerTrack
{
    u8 flags;
    u8 wait;
    u8 patternLevel;
    u8 repN;
    u8 gateTime;
    u8 key;
    u8 velocity;
    u8 runningStatus;
    u8 keyM;
    u8 pitM;
    s8 keyShift;
    s8 keyShiftX;
    s8 tune;
    u8 pitX;
    s8 bend;
    u8 bendRange;
    u8 volMR;
    u8 volML;
    u8 vol;
    u8 volX;
    s8 pan;
    s8 panX;
    s8 modM;
    u8 mod;
    u8 modT;
    u8 lfoSpeed;
    u8 lfoSpeedC;
    u8 lfoDelay;
    u8 lfoDelayC;
    u8 priority;
    u8 echoVolume;
    u8 echoLength;
    M4A_PTR(struct SoundChannel *) chan;
    struct ToneData tone;
    u8 gap[10];
    u16 unk_3A;
    u32 unk_3C;
    M4A_PTR(u8 *) cmdPtr;
    M4A_PTR(u8 *) patternStack[3];
};

#define MUSICPLAYER_STATUS_TRACK 0x0000ffff
#define MUSICPLAYER_STATUS_PAUSE 0x80000000

#define MAX_MUSICPLAYER_TRACKS 16

#define TEMPORARY_FADE  0x0001
#define FADE_IN         0x0002
#define FADE_VOL_MAX    64
#define FADE_VOL_SHIFT  2

struct MusicPlayerInfo
{
    M4A_PTR(struct SongHeader *) songHeader;
    u32 status;
    u8 trackCount;
    u8 priority;
    u8 cmd;
    u8 unk_B;
    u32 clock;
    u8 gap[8];
    M4A_PTR(u8 *) memAccArea;
    u16 tempoD;
    u16 tempoU;
    u16 tempoI;
    u16 tempoC;
    u16 fadeOI;
    u16 fadeOC;
    u16 fadeOV;
    M4A_PTR(struct MusicPlayerTrack *) tracks;
    M4A_PTR(struct ToneData *) tone;
    u32 ident;
    u32 func;
    u32 intp;
};

struct MusicPlayer
{
    M4A_PTR(struct MusicPlayerInfo *) info;
    M4A_PTR(struct MusicPlayerTrack *) track;
    u8 unk_8;
    u16 unk_A;
};

struct Song
{
    M4A_PTR(struct SongHeader *) header;
    u16 ms;
    u16 me;
};

extern const struct MusicPlayer gMPlayTable[];
extern const struct Song gSongTable[];

#ifdef PLATFORM_PC
_Static_assert(sizeof(struct ToneData) == 0xC, "GBA layout");
_Static_assert(sizeof(struct CgbChannel) == 0x40, "GBA layout");
_Static_assert(sizeof(struct SoundChannel) == 0x40, "GBA layout");
_Static_assert(sizeof(struct MusicPlayerTrack) == 0x50, "GBA layout");
_Static_assert(sizeof(struct MusicPlayerInfo) == 0x40, "GBA layout");
_Static_assert(sizeof(struct MusicPlayer) == 0xC, "GBA layout");
_Static_assert(sizeof(struct Song) == 0x8, "GBA layout");

// gMPlayTable (ROM) holds the GBA addresses of the music players' RAM
// structures; natively those are ordinary variables elsewhere, so the table's
// info/track fields are translated by player index (src/m4a.c).
struct MusicPlayerInfo *Port_M4aMPlayInfo(u32 index);
struct MusicPlayerTrack *Port_M4aMPlayTrack(u32 index);
#define M4A_MPLAY_INFO(index)  Port_M4aMPlayInfo(index)
#define M4A_MPLAY_TRACK(index) Port_M4aMPlayTrack(index)
#define M4A_SONG_HEADER(n)     M4A_GET(struct SongHeader *, gSongTable[n].header)

// The music players' RAM (GBA: sound/music_player_table.s, ldscript).
extern struct MusicPlayerInfo gMPlayInfo_BGM1;
extern struct MusicPlayerInfo gMPlayInfo_BGM2;
extern struct MusicPlayerInfo gMPlayInfo_SE1_SYS1;
extern struct MusicPlayerInfo gMPlayInfo_SE2_SYS2;
extern struct MusicPlayerInfo gMPlayInfo_SE3_BMP1;
extern struct MusicPlayerInfo gMPlayInfo_SE4_BMP2;
extern struct MusicPlayerInfo gMPlayInfo_SE5_BMP3;
extern struct MusicPlayerInfo gMPlayInfo_SE6_BMP4;
extern struct MusicPlayerInfo gMPlayInfo_SE7_EVT;
extern struct MusicPlayerTrack gMPlayTrack_BGM1[8];
extern struct MusicPlayerTrack gMPlayTrack_BGM2[8];
extern struct MusicPlayerTrack gMPlayTrack_SE1_SYS1[2];
extern struct MusicPlayerTrack gMPlayTrack_SE2_SYS2[2];
extern struct MusicPlayerTrack gMPlayTrack_SE3_BMP1[4];
extern struct MusicPlayerTrack gMPlayTrack_SE4_BMP2[4];
extern struct MusicPlayerTrack gMPlayTrack_SE5_BMP3[5];
extern struct MusicPlayerTrack gMPlayTrack_SE6_BMP4[4];
extern struct MusicPlayerTrack gMPlayTrack_SE7_EVT[2];

// port/game/m4a_asm.c: DMA 1/2 restart their transfer at the start of
// soundInfo->pcmBuffer (m4aSoundVSyncOn, m4aSoundVSync).
void Port_M4aDmaRestart(void);
#else
#define M4A_MPLAY_INFO(index)  (gMPlayTable[index].info)
#define M4A_MPLAY_TRACK(index) (gMPlayTable[index].track)
#define M4A_SONG_HEADER(n)     (gSongTable[n].header)
#endif



extern u8 gMPlayMemAccArea[];

//u8 gPokemonCrySong[52];
//u8 gPokemonCrySongs[52 * MAX_POKEMON_CRIES];

#define MAX_POKEMON_CRIES 2

extern struct PokemonCrySong gPokemonCrySong;
extern struct PokemonCrySong gPokemonCrySongs[];

extern struct MusicPlayerInfo gPokemonCryMusicPlayers[];
extern struct MusicPlayerTrack gPokemonCryTracks[];

#ifndef PLATFORM_PC
extern char SoundMainRAM[];
#endif

extern void *gMPlayJumpTable[];

typedef void (*XcmdFunc)(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
extern const XcmdFunc gXcmdTable[];

extern struct CgbChannel gCgbChans[];

extern const u8 gScaleTable[];
extern const u32 gFreqTable[];
extern const u16 gPcmSamplesPerVBlankTable[];

extern const u8 gCgbScaleTable[];
extern const s16 gCgbFreqTable[];
extern const u8 gNoiseTable[];

extern const struct PokemonCrySong gPokemonCrySongTemplate;

extern const struct ToneData voicegroup_pokemon_cry;

#ifdef PLATFORM_PC
// Linker script constants on the GBA (ldscript.txt).
#define NUM_MUSIC_PLAYERS 9
#define MAX_LINES 0
#else
extern char gNumMusicPlayers[];
extern char gMaxLines[];

#define NUM_MUSIC_PLAYERS ((u16)gNumMusicPlayers)
#define MAX_LINES ((u32)gMaxLines)
#endif

u32 umul3232H32(u32 multiplier, u32 multiplicand);
void SoundMain(void);
#ifdef PLATFORM_PC
void SoundMainBTM(void *addr);
void MPlayMain(struct MusicPlayerInfo *mplayInfo);
#else
void SoundMainBTM(void);
void MPlayMain(void);
#endif
void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track);
void RealClearChain(void *x);

void MPlayContinue(struct MusicPlayerInfo *mplayInfo);
void MPlayStart(struct MusicPlayerInfo *mplayInfo, struct SongHeader *songHeader);
void m4aMPlayStop(struct MusicPlayerInfo *mplayInfo);
void FadeOutBody(struct MusicPlayerInfo *mplayInfo);
void TrkVolPitSet(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track);
void MPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed);
void ClearChain(void *x);
void Clear64byte(void *addr);
void SoundInit(struct SoundInfo *soundInfo);
void MPlayExtender(struct CgbChannel *cgbChans);
void m4aSoundMode(u32 mode);
void MPlayOpen(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track, u8 a3);
void CgbSound(void);
void CgbOscOff(u8);
u32 MidiKeyToCgbFreq(u8, u8, u8);
void DummyFunc(void);
void MPlayJumpTableCopy(void **mplayJumpTable);
void SampleFreqSet(u32 freq);
void m4aSoundVSyncOn(void);
void m4aSoundVSyncOff(void);

void ClearModM(struct MusicPlayerTrack *track);
void m4aMPlayModDepthSet(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u8 modDepth);
void m4aMPlayLFOSpeedSet(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u8 lfoSpeed);

struct MusicPlayerInfo *SetPokemonCryTone(struct ToneData *tone);
void SetPokemonCryVolume(u8 val);
void SetPokemonCryPanpot(s8 val);
void SetPokemonCryPitch(s16 val);
void SetPokemonCryLength(u16 val);
void SetPokemonCryRelease(u8 val);
void SetPokemonCryProgress(u32 val);
int IsPokemonCryPlaying(struct MusicPlayerInfo *mplayInfo);
void SetPokemonCryChorus(s8 val);
void SetPokemonCryStereo(u32 val);
void SetPokemonCryPriority(u8 val);

// sound command handler functions
void ply_fine(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_goto(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_patt(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_pend(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_rept(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_memacc(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_prio(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_tempo(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_keysh(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_voice(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_vol(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_pan(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_bend(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_bendr(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_lfos(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_lfodl(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_mod(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_modt(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_tune(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_port(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xcmd(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_endtie(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
#ifdef PLATFORM_PC
// Called through soundInfo->plynote with the note length index first.
void ply_note(u32 noteLengthIndex, struct MusicPlayerInfo *, struct MusicPlayerTrack *);
#else
void ply_note(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
#endif

// extended sound command handler functions
void ply_xxx(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xwave(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xtype(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xatta(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xdeca(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xsust(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xrele(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xiecv(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xiecl(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xleng(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xswee(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xcmd_0C(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
void ply_xcmd_0D(struct MusicPlayerInfo *, struct MusicPlayerTrack *);

#endif // GUARD_M4A_INTERNAL_H
