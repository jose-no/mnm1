/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Drum machine fork: 2026 DEADACTIVE */
/* MNM-1 UI pages: a family per page button (pressing it again steps through its pages), and what
 * the four knobs edit on each page. MNM-1's data pages have 8 knobs (2 x 4); the FM-1 has 4: each is two
 * pages here, its upper row (tab, half 0: KNOB 1-4 = its knobs 1-4) and its lower (half 1: its knobs 5-8), and the
 * screen shows all 8 with the row the knobs turn lit and the page's tab in the strip SYN AMP FLT EFX LF1 LF2 LF3.
 * EDIT: SYNTH (SELECT picks the machine), AMP, then TRACK (machine, note, routing, the shared buses' sends); ENV:
 * FILTER; FX: EFFECTS, then the SLICER and the shared buses; LFO: LFO 1, 2, 3; ARP: the arpeggiator. */
enum { FAM_HOME, FAM_SND, FAM_ENV, FAM_FX, FAM_SEQ, FAM_GLO, FAM_SAVE, FAM_GRIDS, FAM_LFO, FAM_MIX, FAM_COUNT };
enum { SC_TRACK, SC_GLOBAL, SC_GRID, SC_MIX, SC_SEQOPT, SC_PERFOPT, SC_GENOPT };   /* knobs edit: the selected track, song.g, the STEP grid, the mixer; SC_GENOPT: GEN (gen_ui.h) */
enum { GR_NONE, GR_MODEL, GR_FX, GR_SLCR, GR_GRID, GR_STEPS, GR_SLOTS, GR_MIX, GR_GRIDS, GR_COMP, GR_LFO, GR_RESON /* MNM-1: the AMP envelope */, GR_MOTION, GR_SONG, GR_FILTER,
       GR_LDEST, GR_ARP, GR_MNM };                  /* GR_MNM: the 8-knob view's lower row and strip (ui_draw.c) */
enum { TAB_NONE, TAB_SYN, TAB_AMP, TAB_FLT, TAB_EFX, TAB_LF1, TAB_LF2, TAB_LF3, TAB_N };
static const char *const TAB_NAME[TAB_N] = {"", "SYN", "AMP", "FLT", "EFX", "LF1", "LF2", "LF3"};

#define SND_SLOT 0xC0u                            /* EDIT page ids: SND_SLOT + n = the track's n-th sound knob */

typedef struct {
    const char *title;
    uint8_t fam, scope, graph;
    uint8_t id[4];               /* parameter ids; 0xFF = empty column */
    uint8_t tab, half;           /* an 8-knob page's tab (TAB_*, 0 none) and row (0 upper: knobs 1-4, 1 lower: 5-8) */
} page_t;

#include "seq_options_ui.h"
#include "performance_ui.h"
#include "gen_ui.h"
static int16_t seqopt_value[4];

static const page_t PAGES[] = {
    /* EDIT: SYNTH (the machine's 8 knobs), AMP; then TRACK, the track's list (snd_list: MODEL, NOTE, routing), 4 a page,
     * a page past its end not shown */
    {"SYNTH", FAM_SND, SC_TRACK, GR_MNM, {P_E0, P_E1, P_E2, P_E3}, TAB_SYN, 0},
    {"SYNTH", FAM_SND, SC_TRACK, GR_MNM, {P_E4, P_E5, P_E6, P_E7}, TAB_SYN, 1},
    {"AMP", FAM_SND, SC_TRACK, GR_MNM, {P_AATK, P_AHOLD, P_ADEC, P_AREL}, TAB_AMP, 0},
    {"AMP", FAM_SND, SC_TRACK, GR_MNM, {P_DIST, P_LEVEL, P_PAN, P_PORT}, TAB_AMP, 1},
    {"TRACK", FAM_SND, SC_TRACK, GR_MODEL, {SND_SLOT + 0, SND_SLOT + 1, SND_SLOT + 2, SND_SLOT + 3}},
    {"TRACK", FAM_SND, SC_TRACK, GR_MODEL, {SND_SLOT + 4, SND_SLOT + 5, SND_SLOT + 6, SND_SLOT + 7}},
    {"TRACK", FAM_SND, SC_TRACK, GR_MODEL, {SND_SLOT + 8, SND_SLOT + 9, SND_SLOT + 10, SND_SLOT + 11}},
    /* ENV: FILTER (filter.c) */
    {"FILTER", FAM_ENV, SC_TRACK, GR_MNM, {P_FCUT, P_FWDTH, P_FRESO, P_FLPQ}, TAB_FLT, 0},
    {"FILTER", FAM_ENV, SC_TRACK, GR_MNM, {P_FATK, P_FDEC, P_FENV, P_FWOFS}, TAB_FLT, 1},
    /* FX: EFFECTS (EQ, SRR, the track's delay: fx.c), then the SLICER and the shared buses (their sends: TRACK) */
    {"EFFECTS", FAM_FX, SC_TRACK, GR_MNM, {P_EQF, P_EQG, P_SRR, P_DTIM}, TAB_EFX, 0},
    {"EFFECTS", FAM_FX, SC_TRACK, GR_MNM, {P_DLY, P_DFB, P_DBAS, P_DWID}, TAB_EFX, 1},
    {"SLICER", FAM_FX, SC_TRACK, GR_SLCR, {P_SLCR, P_SLPAT, P_SLRATE, P_SLDEPTH}},
    {"REVERB", FAM_FX, SC_GLOBAL, GR_NONE, {G_RTYPE, G_RSIZE, G_RDAMP, 0xFF}},   /* TYPE: ROOM / SPRING */
    {"CHORUS", FAM_FX, SC_GLOBAL, GR_NONE, {G_CRATE, G_CDEPTH, 0xFF, 0xFF}},
    {"COMP", FAM_HOME, SC_GLOBAL, GR_COMP, {G_CSRC, G_CTHR, G_CRAT, G_CREL}},   /* HOME 2/3; keys: DUCK per track */
    {"COMP", FAM_HOME, SC_GLOBAL, GR_COMP, {G_CATK, G_CKNEE, G_CMKUP, G_CGHOST}},   /* HOME 3/3 */
    {"STEP", FAM_SEQ, SC_GRID, GR_GRID, {0xFF, 0xFF, 0xFF, 0xFF}},   /* KNOB 1: bank */
    {"PATTERN", FAM_SEQ, SC_TRACK, GR_STEPS, {P_SLEN, P_SDIV, P_SSWING, P_NLEN}},   /* GATE: a note's length */
    {"MOTION", FAM_SEQ, SC_TRACK, GR_MOTION, {0xFF, 0xFF, 0xFF, 0xFF}},   /* PLAY EVENT - CLEAR (motion.c) */
    {"SONG", FAM_SEQ, SC_GLOBAL, GR_SONG, {0xFF, 0xFF, 0xFF, 0xFF}},   /* ROW SLOT REPS LOOP (song.c) */
    {"GLOBAL", FAM_GLO, SC_GLOBAL, GR_NONE, {G_BPM, G_SWING, G_CLOCK, G_DRCH}},
    {"SYSTEM", FAM_GLO, SC_GLOBAL, GR_NONE, {G_MIDI, G_SYNC, G_ROUTE, G_INFO}},
    {"GLOBAL", FAM_GLO, SC_GLOBAL, GR_NONE, {G_MUTEBAR, G_METRO, 0xFF, 0xFF}},   /* device settings (settings.mutebar,
                                                                                * fx_metro) */
    {"PROJECT", FAM_SAVE, SC_GLOBAL, GR_SLOTS, {G_SLOT, G_NAME, G_LOAD, G_SAVE}},   /* NAME: rename (ui_name.c) */
    {"TOOLS", FAM_SAVE, SC_GLOBAL, GR_NONE, {G_CLRSEQ, G_INITSND, G_CLRALL, G_INITALL}},   /* track; all */
    {"ARP", FAM_GRIDS, SC_TRACK, GR_ARP, {P_ARPM, P_ARPS, P_ARPR, P_ARPC}},   /* arp.c */
    /* LFO: LFO 1, 2, 3 (lfo.c), its knobs: PAGE DEST TRIG WAVE / MULT SPD INTL DPTH */
#define LFO_PAGES(b, tab)                                                                                     \
    {"LFO", FAM_LFO, SC_TRACK, GR_MNM, {b + LF_PAGE, b + LF_DEST, b + LF_TRIG, b + LF_WAVE}, tab, 0},          \
    {"LFO", FAM_LFO, SC_TRACK, GR_MNM, {b + LF_MULT, b + LF_SPD, b + LF_INTL, b + LF_DPTH}, tab, 1}
    LFO_PAGES(P_LFO1, TAB_LF1),
    LFO_PAGES(P_LFO2, TAB_LF2),
    LFO_PAGES(P_LFO3, TAB_LF3),
#undef LFO_PAGES
    {"DESTROY", FAM_FX, SC_TRACK, GR_NONE, {P_BITS, P_FOLD, P_XOR, P_SPU}},
    {"MOD FX", FAM_FX, SC_TRACK, GR_NONE, {P_MODT, P_MODR, P_MODD, P_MODM}},
    {"LOFI", FAM_FX, SC_GLOBAL, GR_NONE, {G_TAPE, G_ADPC, G_HISS, G_DRIVE}},
    {"KIT", FAM_SAVE, SC_GLOBAL, GR_SLOTS, {G_KSLOT,G_KNAME,G_KLOAD,G_KSAVE}},
    {"ARP MODE", FAM_GRIDS, SC_SEQOPT, GR_NONE, {SO_MODE,SO_ORDER,SO_SPEED,SO_RANGE}},
    {"ARP TRIG", FAM_GRIDS, SC_SEQOPT, GR_NONE, {SO_OJMP,SO_AMP,SO_FLT,SO_LFO}},
    {"ARP RHYTHM", FAM_GRIDS, SC_SEQOPT, GR_NONE, {SO_LENGTH,SO_RSTEP,SO_RON,SO_ROFF}},
    {"SWING MASK", FAM_SEQ, SC_SEQOPT, GR_NONE, {SO_SWING,SO_SSTEP,SO_SON,0xFF}},
    {"TRANSPOSE", FAM_SEQ, SC_SEQOPT, GR_NONE, {SO_TRANS,SO_SCALE,SO_KEY,SO_PTRANS}},
    /* GEN (gen.h): REC on these pages draws a new seed and writes the phrase again */
    {"GEN", FAM_SEQ, SC_GENOPT, GR_NONE, {GO_KIND,GO_MARB,GO_OCT,GO_FLW}},
    {"GEN CHORD", FAM_SEQ, SC_GENOPT, GR_NONE, {GO_CHRD,GO_BARS,GO_LOCK,GO_UNDO}},
    {"ASSIGN VEL", FAM_SND, SC_PERFOPT, GR_NONE, {PO_VDEST1,PO_VAMT1,PO_VDEST2,PO_VAMT2}},
    {"ASSIGN KEY", FAM_SND, SC_PERFOPT, GR_NONE, {PO_KDEST1,PO_KAMT1,PO_KDEST2,PO_KAMT2}},
    {"TRIG TRACK", FAM_SND, SC_PERFOPT, GR_NONE, {PO_ROUTE,PO_HP,PO_LP,0xFF}},
    {"MULTI MODE", FAM_GRIDS, SC_PERFOPT, GR_NONE, {PO_MMODE,PO_MCHANNEL,PO_MKEY,PO_MTRACK}},
    {"MULTI ENV", FAM_GRIDS, SC_PERFOPT, GR_NONE, {PO_MATK,PO_MDEC,PO_MSUS,PO_MREL}},
    {"MULTI TIME", FAM_GRIDS, SC_PERFOPT, GR_NONE, {PO_MTIMING,PO_MPORT,0xFF,0xFF}},
    {"SONG RANGE", FAM_SEQ, SC_GLOBAL, GR_SONG, {0xFF,0xFF,0xFF,0xFF}, TAB_NONE, 1},
    {"SONG MIX", FAM_SEQ, SC_GLOBAL, GR_SONG, {0xFF,0xFF,0xFF,0xFF}, TAB_NONE, 2},
    {"SONG FLOW", FAM_SEQ, SC_GLOBAL, GR_SONG, {0xFF,0xFF,0xFF,0xFF}, TAB_NONE, 3},
    {"SONG FILE", FAM_SEQ, SC_GLOBAL, GR_SONG, {0xFF,0xFF,0xFF,0xFF}, TAB_NONE, 4},
    /* HOME's TRACKS mode (spec §3: a HOME tap on HOME; no button of its own): TRACK LEVEL LEN PAN */
    {"TRACKS", FAM_MIX, SC_MIX, GR_MIX, {0xFF, 0xFF, 0xFF, 0xFF}},
};
#define NPAGES (sizeof(PAGES) / sizeof(PAGES[0]))

/* the button (and LED) of each family; SYNTH and AMP are EDIT's, FILTER ENV's, the ARP page ARP's (FAM_GRIDS: the family Grids had),
 * the COMP pages (HOME 2/3, 3/3) and TRACKS (HOME's TRACKS mode) HOME's */
static const uint8_t FAM_BTN[FAM_COUNT] = {B_HOME, B_EDIT, B_ENV, B_FX, B_SEQ, B_GLO, B_SAVE, B_ARP, B_LFO, B_HOME};

/* the TRACK list of track t: MODEL, NOTE, OUT (an FX machine's IN, GATE), CHOKE, PRIO, the shared REVERB and CHORUS
 * buses' sends (the machine's knobs: SYNTH; LEVEL, PAN: AMP; the track's delay: EFFECTS); returns how many */
static uint32_t snd_list(const track_t *t, uint8_t *ids)
{
    const dmodel_t *m = &DMODELS[(uint32_t)t->p[P_MODEL] % NMODELS];
    uint32_t n = 0;
    ids[n++] = P_MODEL;
    ids[n++] = P_NOTE;
    ids[n++] = P_OUT;                                    /* routing (fx.c): the bus; an FX machine's input, gate */
    if (m->fx) {
        ids[n++] = P_IN;
        ids[n++] = P_FXG;
    }
    ids[n++] = P_CHOKE;
    ids[n++] = P_PRIO;
    ids[n++] = P_REV;
    ids[n++] = P_CHOR;
    if (m->mm >= 2u) {
        ids[n++] = P_PTCH;
        ids[n++] = P_WBNK;
    }
    return n;
}

/* the parameter of a column: an EDIT slot is the selected track's sound list; GRIDS 1/2 in EUCLID mode turns
 * LEN K / S / H where MAP has X / Y / CHAOS; 0xFF = empty */
static uint32_t page_id(const page_t *pg, uint32_t slot)
{
    uint32_t id = pg->id[slot & 3u];
    if (id >= SND_SLOT && id < SND_SLOT + 16u) {
        uint8_t ids[16];
        uint32_t n = snd_list(TSEL, ids);
        return id - SND_SLOT < n ? ids[id - SND_SLOT] : 0xFFu;
    }
    if (pg->scope == SC_TRACK && DMODELS[(uint32_t)TSEL->p[P_MODEL] % NMODELS].mm >= 2u) {
        if (pg->tab == TAB_AMP && id == P_LEVEL) id = P_MVOL;
        if (pg->tab >= TAB_LF1 && pg->tab <= TAB_LF3) {
            uint32_t l = pg->tab - TAB_LF1, base = lfo_base(l);
            if (id == base + LF_PAGE) id = P_L1TRK + l;
            if (id == base + LF_INTL) id = P_L1PHS + l;
        }
    }
    if (pg->graph == GR_GRIDS && id >= G_GX && id <= G_GCHAOS && song.g[G_GMODE])
        id = G_GLEN1 + (id - G_GX);
    return id;
}

static const param_desc_t *page_desc(const page_t *pg, uint32_t slot, int16_t **valp)
{
    uint32_t id = page_id(pg, slot);
    *valp = 0;
    if (id == 0xFFu || pg->scope == SC_GRID || pg->scope == SC_MIX)
        return 0;
    if(pg->scope==SC_PERFOPT){
        if(id>=PO_COUNT)return 0;
        seqopt_value[slot&3u]=(int16_t)perfui_get(TSEL,&song.multi,id);
        *valp=&seqopt_value[slot&3u];return &PO_DESC[id];
    }
    if(pg->scope==SC_GENOPT){
        if(id>=GO_COUNT)return 0;
        seqopt_value[slot&3u]=(int16_t)genopt_get(TSEL,id);
        *valp=&seqopt_value[slot&3u];return &GO_DESC[id];
    }
    if(pg->scope==SC_SEQOPT){
        if(id>=SO_COUNT)return 0;
        seqopt_value[slot&3u]=(int16_t)seqopt_get(TSEL,&song.seq,id);
        *valp=&seqopt_value[slot&3u];return &SO_DESC[id];
    }
    if (pg->scope == SC_GLOBAL && id >= G_COUNT) {   /* TOOLS actions, MUTE, METRO */
        id = id < G_ACT_END ? id - G_COUNT : 0u;
        *valp = &song.act[id];
        return &GP_ACT[id];
    }
    if (pg->scope == SC_GLOBAL) {
        *valp = &song.g[id];
        return &GP[id];
    }
    *valp = &TSEL->p[id];
    return track_desc(TSEL, id);
}
