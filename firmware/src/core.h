/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Drum machine fork: 2026 DEADACTIVE */
/* MNM-1 core types: 8 tracks; each plays one machine (a mono synth voice, mm_synth.c, or a drum
 * model, 1 or 2 voices) through its AMP envelope, the dual FILTER, SRR and Felucca's per-track DIST / SLICER / sends
 * (fx.c). */
#include <stdint.h>
#define NTRK 6                   /* tracks (MNM-1: six) */
#define NPART NTRK               /* fx.c / slicer.c: every track is mixed the same way */
#define NDV 2                    /* voices per track; a model uses 1 or 2 */
#define NSTEP 64
#define HALF_FRAMES 256          /* I2S half buffer: 5.8 ms at 44.1 kHz */
#define UP_SLOTS 32u             /* user preset slots (storage layout; presets are off in M1) */

/* ------------------------------------------------------- parameters --- */
enum {
    F_INT, F_PCT, F_BIPCT, F_TIME, F_LFOHZ, F_CUTOFF, F_DB, F_SEMI, F_ENUM, F_BPM, F_NOTE,
    F_ONOFF, F_OCT, F_STEPS, F_CTHR, F_CRAT, F_CATK, F_CREL, F_CMKUP, F_LDEST1, F_LDEST2, F_LDEST3, F_LMULT, F_EQG,
    F_NOTEO, F_RDECAY, F_FDEC,
    F_AHOLD, F_ADEC, F_ASSIGN                /* MNM-1's AMP: a time, 127 GATE / INF */
};

typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min, max, def;
    const char *const *names;   /* F_ENUM */
    const char *unit;           /* F_INT / F_ENUM optional unit */
} param_desc_t;
#define PD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}
#define PE(l, n, df) {l, F_ENUM, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df, n, 0}

/* an LFO's knobs (lfo.c): PAGE DEST TRIG WAVE / MULT SPD INTL DPTH */
enum { LF_PAGE, LF_DEST, LF_TRIG, LF_WAVE, LF_MULT, LF_SPD, LF_INTL, LF_DPTH, LF_N };
/* its WAVEs: five shapes, RND, the five mirrored (upside down) */
enum { LW_TRI, LW_SINE, LW_SQR, LW_SAW, LW_EXP, LW_RND, LW_ITRI, LW_ISINE, LW_ISQR, LW_ISAW, LW_IEXP, LW_COUNT };
enum { LT_FREE, LT_TRIG, LT_HOLD, LT_ONE, LT_HALF };   /* its TRIG modes */
enum { LP_SYN, LP_AMP, LP_FLT, LP_EFX, LP_LF1, LP_LF2, LP_LF3, LP_PTCH, LP_N };   /* its PAGEs (PTCH: the pitch) */
#define NLFO 3u

enum {                          /* per-track parameters */
    P_MODEL,
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,   /* the model's: TUNE DECAY TONE CHAR + 4 extras */
    P_LEVEL, P_PAN, P_MUTE, P_CHOKE, P_NOTE,
    P_DIST, P_CHOR, P_DLY, P_REV,   /* DIST; the CHORUS and REVERB buses' sends; P_DLY: EFFECTS' DSND, the track's own
                                     * delay's send (fx.c track_delay; bipolar: + ping-pong) */
    P_SLCR, P_SLPAT, P_SLRATE, P_SLDEPTH,
    P_SLEN, P_SDIV, P_SSWING,
    P_AATK, P_AHOLD, P_ADEC, P_AREL, P_PORT,         /* AMP (mm_synth.c): ATTACK HOLD DECAY RELEASE, PORTAMENTO */
    P_SRC,                       /* what the track plays: 0 its steps, 1..3 a Grids channel (kick, snare, hats) */
    P_DUCK,                      /* M3: the COMP source ducks this track */
    P_LFO1,                      /* LFO 1: LF_N knobs (lfo.c) */
    P_LFO2 = P_LFO1 + LF_N,      /* LFO 2 */
    P_FWDTH = P_LFO2 + LF_N,     /* FILTER's WIDTH (TYPE DUAL: the low-pass BASE + WIDTH over the high-pass) */
    P_FATK,                      /* FILTER's envelope ATTACK */
    P_SRR,                       /* EFFECTS: sample rate reduction (fx.c track_srr) */
    P_NLEN,                      /* a synth machine's note LENGTH: 1..127 % of the step (127: tied to the next) */
    P_ARPM, P_ARPS, P_ARPR,      /* ARP (arp.c): MODE, SPEED (a DIV), RANGE (octaves) */
    P_FLPQ, P_FCUT, P_FRESO, P_FENV, P_FDEC,    /* FILTER (filter.c): LPQ, BASE, HPQ, BOFS (the envelope on BASE),
                                                 * DEC */
    P_PRIO,                      /* 0 HIGH, 1 LOW: a LOW track's voice goes first (the voice cap, a CPU shed; FDRC) */
    P_LFO3,                      /* LFO 3: LF_N knobs */
    P_LFO3_END = P_LFO3 + LF_N - 1,
    P_ARPC,                      /* ARP's CHORD (arp.c ARP_CHORD) */
    P_OUT,                       /* routing (fx.c): the mix bus the track goes to, AB CD EF or OFF */
    P_IN,                        /* an FX machine's input (mm_fx.c): NEIB (the track before) or a bus AB CD EF */
    P_FXG,                       /* an FX machine's gate: ON (always passes) or TRIG (its notes open the AMP) */
    P_FWOFS,                     /* FILTER's WOFS: the envelope on WDTH */
    P_EQF, P_EQG,                /* EFFECTS (fx.c track_eq): the EQ's frequency, gain (64: 0 dB, off) */
    P_DTIM, P_DFB, P_DBAS, P_DWID,   /* EFFECTS: the track's delay (fx.c track_delay): TIME (1/256 notes), FEEDBACK
                                      * (64: unity), the loop's high-pass BASE and low-pass WIDTH over it */
    P_MVOL,                     /* calibrated sound VOL; mixer LEVEL stays separate */
    P_BITS, P_FOLD, P_XOR, P_SPU,
    P_MODT, P_MODR, P_MODD, P_MODM,
    P_WBNK,
    P_L1TRK, P_L2TRK, P_L3TRK,
    P_L1PHS, P_L2PHS, P_L3PHS,
    P_PTCH,                     /* calibrated pitch offset, raw 64 = neutral */
    P_COUNT
};
#define P_NSTORED P_COUNT        /* the parameters in a project's block: all (MNM-1's 6 tracks leave room) */
enum { BUS_AB, BUS_CD, BUS_EF, BUS_OFF };   /* P_OUT */
enum { IN_NEIB, IN_AB, IN_CD, IN_EF };       /* P_IN */

enum {                          /* global parameters */
    G_BPM, G_SWING, G_CLOCK,
    G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX,
    G_RSIZE, G_RDAMP, G_CRATE, G_CDEPTH,
    G_MIDI, G_SYNC, G_ROUTE, G_INFO,
    G_SLOT, G_NAME, G_LOAD, G_SAVE,
    G_CLRSEQ, G_INITSND,
    G_DRCH,                      /* MIDI channel of the drum tracks, 1..16 */
    G_GMODE, G_GX, G_GY, G_GCHAOS,                   /* Grids (grids.c): MAP / EUCLID, the map point, chaos */
    G_GFILL1, G_GFILL2, G_GFILL3,                    /* fill per channel (kick, snare, hats) */
    G_GLEN1, G_GLEN2, G_GLEN3,                       /* Euclidean length per channel, 1..32 sixteenths */
    G_CSRC, G_CTHR, G_CRAT, G_CREL,                  /* COMP (comp.c): source track (0 off), threshold, ratio, release */
    G_CATK, G_CKNEE, G_CMKUP,                        /* attack, soft knee, makeup (127 = limiter) */
    G_CGHOST,                                        /* a muted / any source heard: CG_MUTE CG_KEEP CG_HIDE (fx.c) */
    G_RTYPE,                                         /* REVERB TYPE (fx.c): 0 ROOM, 1 SPRING (sound pack, FDR7) */
    G_TAPE, G_ADPC, G_HISS, G_DRIVE,
    G_COUNT
};
enum { CG_MUTE, CG_KEEP, CG_HIDE };                 /* GHOST: muted = muted; muted still keys; never heard, keys */
enum { G_CLRALL = G_COUNT, G_INITALL, G_MUTEBAR, G_METRO, G_KSLOT, G_KNAME, G_KLOAD, G_KSAVE, G_ACT_END };   /* TOOLS actions, the MUTE and METRO settings: shown
                                                     * like globals, not stored in projects */

/* ----------------------------------------------------------- voices --- */
typedef struct {                 /* sample playback state (eng_sample.c sample_next, dm_sample.c) */
    uint32_t ph[3];              /* ph[0] position, ph[1] fraction Q16 */
    int32_t s[8];                /* s[0] predictor, s[1] step index, s[2..3] interpolation, s[4] zone,
                                  * s[5] step Q16, s[6] tone state */
} voice_t;

typedef struct { int32_t a1, a2, a3, k; } dsvf_t;   /* dm_dsp.c: SVF with LP / BP / HP */
#include "dm_state.h"

typedef struct {                 /* one drum voice; the fields' meaning is the model's */
    uint8_t active, vel;
    uint32_t age;                /* hit order (voice stealing) */
    uint32_t t;                  /* samples since the hit (models that need it) */
    uint32_t ph[3], inc[3];      /* oscillator phases and increments */
    int32_t env[3];              /* envelopes, Q24 (1 << 24 = full) */
    uint32_t k[3];               /* their per-sample factors, Q16 (DECAY_K) */
    dsvf_t c[2];                 /* filters */
    int32_t f[4];                /* filter states (2 per filter) */
    int32_t x[4];                /* model scratch: gains, amounts */
    int32_t rng;                 /* noise state, seeded per hit */
    int32_t last;                /* last sample added to the output (declick when cut) */
    voice_t sv;                  /* sample playback (SAMPLE model, layer) */
    dm_state_t ms;               /* M1-C model state (dm_state.h) */
} dvoice_t;

struct track;
typedef struct {
    const char *name;            /* <= 5 chars (UI value) */
    uint8_t voices;              /* 1 or 2 */
    uint8_t choke;               /* default choke group, 0 = none */
    param_desc_t edit[8];        /* P_E0..P_E7: TUNE DECAY TONE CHAR + 4 extras */
    void (*trigger)(struct track *t, dvoice_t *v);
    void (*render)(struct track *t, dvoice_t *v, int32_t *out, uint32_t n);
    uint8_t weight;              /* what a voice counts toward DRUM_MAXV: 0 / 1 = 1, 2 = a heavy model (M1-C) */
    void (*glide)(struct track *t, dvoice_t *v);         /* ACID: the voice moves to the track's note, no restart */
    void (*gate)(struct track *t, dvoice_t *v, int open); /* ACID: 0 = the gate closes (release) */
    uint8_t mm;                  /* a synth machine (mm_synth.c): the AMP envelope, PORT, LEN are its */
    uint8_t fx;                  /* an FX machine (mm_fx.c): it processes its input (P_IN), makes no sound itself */
} dmodel_t;

/* Pattern-owned options; byte-only layout is shared by flash records. */
typedef struct {
    uint8_t swing[8];
    int8_t arp_offset[16];
    uint8_t arp_mask[2];
    uint8_t arp_mode_order, arp_ojmp, arp_len, arp_flags;
    int8_t transpose;
    uint8_t scale_key, arp_speed;
} seq_options_t;
#define SEQ_NSCALES 9u           /* seq_options_t.scale_key >> 4: --- FIX MAJ MIN DOR PHR MIN5 MAJ5 WHL */
typedef struct {uint8_t swing;int8_t transpose;uint8_t scale,key;} pattern_options_t;
/* GEN (gen.h, ported from Mutable Dreams by Kid Bunka / Christoph Link): a track's phrase generator and its live
 * variation, and the pattern's seed and chord journey. Byte layout shared by flash records. */
typedef struct {
    uint8_t kind;                /* the last phrase written: 0 none, 1 SPARSE, 2 ARP, 3 PULSE, 4 DRIFT */
    uint8_t marbles;             /* 0..100 %: off-beat notes become the root, a third, fourth or fifth */
    int8_t oct;                  /* -3..3: the generated phrase's octave around C4 */
    uint8_t follow;              /* 1: the track follows the chord journey (FLLW) */
} gen_options_t;
typedef struct {
    uint8_t seed[4];             /* little-endian, never 0 (xorshift): gen_seed / gen_seed_set */
    uint8_t journey, bars, lock, rsv;   /* journey 0 OFF, 1..4; bars a chord 1..8; lock: variation repeats */
} gen_pattern_t;
_Static_assert(sizeof(gen_options_t)==4&&sizeof(gen_pattern_t)==8,"GEN wire size");
#define GEN_SEED_DEFAULT 0x2545F491u
static uint32_t gen_seed(const gen_pattern_t *g)
{return g->seed[0]|(uint32_t)g->seed[1]<<8|(uint32_t)g->seed[2]<<16|(uint32_t)g->seed[3]<<24;}
static void gen_seed_set(gen_pattern_t *g,uint32_t v)
{if(!v)v=GEN_SEED_DEFAULT;g->seed[0]=(uint8_t)v;g->seed[1]=(uint8_t)(v>>8);g->seed[2]=(uint8_t)(v>>16);g->seed[3]=(uint8_t)(v>>24);}
static void gen_options_default(gen_options_t *g){*g=(gen_options_t){0};}   /* (FLLW off: a drum's NOTE is its tuning) */
static void gen_pattern_default(gen_pattern_t *g){*g=(gen_pattern_t){.bars=2};gen_seed_set(g,GEN_SEED_DEFAULT);}
static int gen_options_valid(const gen_options_t *g)
{return g->kind<=4&&g->marbles<=100&&g->oct>=-3&&g->oct<=3&&g->follow<=1;}
static int gen_pattern_valid(const gen_pattern_t *g)
{return gen_seed(g)&&g->journey<=4&&g->bars>=1&&g->bars<=8&&g->lock<=1&&!g->rsv;}
_Static_assert(sizeof(seq_options_t)==33,"sequence option wire size");
static void seq_options_default(seq_options_t *s)
{
    *s=(seq_options_t){.swing={170,170,170,170,170,170,170,170},.arp_mask={255,255},.arp_len=8,.arp_speed=6};
}
#include "performance_options.h"
/* ------------------------------------------------------------ track --- */
typedef struct {                 /* one sequencer step (4 B, stored as is); all zero = a plain step, off, at C4 */
    uint8_t on : 1, acc : 1;     /* hit; accent (velocity 127, else 96) */
    uint8_t slide : 1, tie : 1;  /* ACID: glide into the next note / hold the sounding note through this step */
    uint8_t rat : 2;             /* RATCH: hits - 1 (0..3) */
    uint8_t pslide : 1;          /* MNM-1: PARAMETER SLIDE, its locks glide to their next (motion.c) */
    uint8_t : 1;
    uint8_t cond;                /* PROB: 0 = 100 %, 1..20 = 0..95 %, 21 = 1-SHOT, 22..56 = A/B (params.c cond_*) */
    int8_t nt;                   /* NOTE - 60 (C4 = 0): a drum's transpose, ACID's note (NT_MIN..NT_MAX) */
    uint8_t rsv;                 /* ST_* event flags; zero preserves legacy ALL */
} step_t;
enum {ST_NOAMP=1,ST_NOFLT=2,ST_NOLFO=4,ST_NOPITCH=8,ST_NOTEOFF=16,ST_MASK=31};
#define NT_MIN (-60)
#define NT_MAX 67
#define GATE_HELD 0xFFFFFFFFu     /* track_t.gate_n: the gate stays open, the next step decides (seq.c acid_step) */

typedef struct { uint32_t ph; int32_t out, run, rnd; uint8_t fresh, stop; } lfo_state_t;   /* lfo.c: phase, the
                                                    * output, the wave running (HOLD samples it), RND's value, restarted,
                                                    * stopped (ONE / HALF at their end) */

typedef struct track {
    int16_t p[P_COUNT];
    uint8_t model;               /* model the voices were started with (a change cuts them) */
    dvoice_t v[NDV];             /* model voices */
    int32_t dtail;               /* declick: last output of cut voices, decaying */
    /* sequencer */
    step_t step[NSTEP];
    seq_options_t seq;
    gen_options_t gen;           /* GEN (gen.h) */
    perf_options_t perf;
    int16_t multi_pitch_ref;
    int8_t nt;                   /* the playing note's offset from C4 (the step's, 0 for MIDI, keys): models read TUNE through dm_tune */
    uint32_t gate_n;             /* ACID: samples until its gate closes, GATE_HELD while the next step decides, 0 closed */
    uint8_t slide_in;            /* ACID: the previous step slides into this one */
    int8_t trig_nt;             /* last sounding note, also when ARP restores its parent note */
    int8_t rat_nt;               /* RATCH: the roll's note (a live hit between its hits leaves it) */
    int8_t key_nt;               /* the note KEYS played or recorded last on this track (0 = C4): a new step's (§2) */
    uint32_t seq_pos;            /* samples into the current step */
    uint16_t seq_idx;
    uint32_t seq_cnt;            /* steps played since PLAY: swing pairs follow it, so any length stays on the bar */
    uint8_t seq_rem;             /* the step length's remainder carried to the next step (seq.c div_period) */
    uint8_t seq_par;             /* the swing pairs' phase: a song's row keeps them on the bar (song.c) */
    uint8_t rskip, rskip_idx;    /* live recording put a hit into the step about to play: skip it once */
    uint32_t rng;                /* PROB: the track's random sequence (LCG), seeded at PLAY */
    uint32_t rat_len;            /* RATCH: the playing roll's step length (samples) */
    uint8_t rat_n, rat_k, rat_vel, rat_flags;   /* its hits, the next hit, their velocity; rat_n 0 = no roll */
    uint16_t gfade;              /* M3 ghost key: how silenced the COMP source's sound is (0 heard .. 32767 muted) */
    lfo_state_t lfo[NLFO];       /* LFO 1 / 2 / 3 runtime (lfo.c) */
    int16_t lpit;                /* the LFOs' PTCH this block, 1/16 semitone (mm_synth.c mm_pitch adds it) */
    int32_t eq_s[2];             /* EQ (fx.c track_eq): its SVF's state */
    uint32_t srr_ph;             /* SRR (fx.c track_srr): the held sample's phase, Q16, and the sample held */
    int32_t srr_x;
    uint8_t arp_on;              /* ARP (arp.c): a run playing */
    uint32_t arp_k;              /* .. its next note's count */
    int8_t arp_nt;               /* the note the ARP plays around (the step's, the key's) */
    uint32_t arp_pos, arp_len;   /* samples into its step, a step's length */
    uint8_t arp_dir;             /* UP-DOWN's direction */
    uint32_t lrng;               /* random generator of S&H / WANDER / RWALK, reseeded at PLAY */
    uint8_t lon, lnum, lpid[4];  /* the LFOs on (bit l), the knobs written this block (at most the 4 DEST slots'), which */
    int16_t lsave[4], lval[4];   /* their set values (restored at the end of the block), the modulated values */
    /* mix runtime (fx.c) */
    int32_t peak;
    int32_t dist_hp, dist_lp1, dist_lp2;   /* DIST insert state */
    uint8_t tail;                /* blocks to mix after the last voice (the DIST tail) */
    int32_t fs[4];               /* FILTER (filter.c): the SVFs' states (DUAL: two) */
    int32_t fenv;                /* its envelope, Q24 (a hit: 0, rising at ATTACK to 1 << 24, then DECAY) */
    uint8_t fvel, fmode, fring;  /* the hit's velocity, the TYPE running (a change: from silence), ringing */
    uint8_t fatk;                /* the envelope's attack is running */
    int32_t mm_p16;              /* a synth machine's last pitch (1/16 semitone, 0 none): PORT glides from it */
    int32_t wow;                 /* ACID: the accent smoother, Q24 (accented notes charge it, track_render decays it
                                  * every block: it carries over consecutive accents, as the 303's) */
    uint16_t pre;                /* presets.c: the preset id the voice was loaded from (PRE_NONE: none; projects: FDRD) */
    uint8_t pre_ed;              /* .. a SOUND knob moved since (preset_touch) */
} track_t;

typedef struct {
    int16_t g[G_COUNT];
    uint8_t playing, seq_mode;
    uint8_t rec;                 /* live recording armed: bit per track */
    uint8_t sel;                 /* selected track 0..NTRK-1: keys, pages, editor */
    int8_t octave;               /* KEYS: the keyboard's octave, -2..+2 (OCT- / OCT+), never stored */
    uint32_t tick;               /* blocks since play */
    uint32_t cpu_q8;             /* audio ISR load, 1/256 */
    uint32_t cpu_rem;            /* its average's remainder (audio.c) */
    uint32_t sheds;              /* overload sheds (audio.c shed_voice): the top bar's CPU icon (ui_draw.c) */
    uint32_t master_q12;
    int32_t batt_raw;            /* smoothed ADC ch3 (battery divider), 0 = not read yet */
    pattern_options_t seq;
    gen_pattern_t gen;           /* GEN: the seed, the chord journey (gen.h) */
    multi_options_t multi;
    int16_t multi_pitch16;
    int16_t act[G_ACT_END-G_COUNT]; /* TOOLS/KIT action knobs, never stored; MUTE: a
                                  * copy of settings.mutebar; METRO: of fx_metro */
    uint8_t mute_q;              /* mutes to flip on the next bar (bit per track; seq.c) */
    uint8_t solo;                /* SOLO (the SMC-Mixer): bit per track, runtime only (trk_silent) */
    uint8_t lfo_in;              /* lfo.c: inside an audio block, between lfo_apply and lfo_restore */
} song_t;

static track_t trk[NTRK];
static song_t song;
#define TSEL (&trk[song.sel])    /* the selected track */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* slot store before the index update */
static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR in main.c) */
/* boot-loop guard (main.c): two boots in a row that die in the first 30 s -> UBOOT */
#define BOOTGUARD_MAGIC 0x42475244u
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));
