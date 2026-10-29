/* SPDX-License-Identifier: GPL-3.0-only
 * GEN's knobs (gen.h): the GEN and GEN CHORD pages (pages.c SC_GENOPT). The track's GEN MARB OCT FLLW, the
 * pattern's CHRD BARS LOCK, and UNDO, an action (GO, then back to --). */
#ifndef MNM_GEN_UI_H
#define MNM_GEN_UI_H
enum { GO_KIND, GO_MARB, GO_OCT, GO_FLW, GO_CHRD, GO_BARS, GO_LOCK, GO_UNDO, GO_COUNT };
static const char *const GO_KINDS[] = {"---", "SPRS", "ARP", "PULS", "DRFT"};   /* SPARSE ARP PULSE DRIFT (4 fit) */
static const char *const GO_CHORDS[] = {"OFF", "1415", "1645", "1741", "1364"};   /* I-IV-I-V, I-VI-IV-V, I-VII-IV-I,
                                                                                    * I-III-VI-IV */
static const char *const GO_UNDOS[] = {"--", "GO"};
static const param_desc_t GO_DESC[GO_COUNT] = {
    [GO_KIND] = PE("GEN", GO_KINDS, 0),   [GO_MARB] = PD("MARB", F_INT, 0, 100, 0),
    [GO_OCT] = PD("OCT", F_INT, -3, 3, 0), [GO_FLW] = PD("FLLW", F_ONOFF, 0, 1, 0),
    [GO_CHRD] = PE("CHRD", GO_CHORDS, 0), [GO_BARS] = PD("BARS", F_INT, 1, 8, 2),
    [GO_LOCK] = PD("LOCK", F_ONOFF, 0, 1, 0), [GO_UNDO] = PE("UNDO", GO_UNDOS, 0)};

static int genopt_get(const track_t *t, unsigned id)
{
    switch (id) {
    case GO_KIND: return t->gen.kind;
    case GO_MARB: return t->gen.marbles;
    case GO_OCT: return t->gen.oct;
    case GO_FLW: return t->gen.follow;
    case GO_CHRD: return song.gen.journey;
    case GO_BARS: return song.gen.bars;
    case GO_LOCK: return song.gen.lock;
    default: return 0;                             /* (UNDO shows --) */
    }
}

/* a knob's new value (ISR off); 1 changed. GEN's phrase writes the track's steps (gen_fill: undo keeps the ones
 * before), --- leaves them; UNDO's GO puts the last ones back (0: nothing to undo) */
static int genopt_set(track_t *t, unsigned id, int value)
{
    if (id >= GO_COUNT || value < GO_DESC[id].min || value > GO_DESC[id].max)
        return 0;
    if (id == GO_UNDO)
        return value ? gen_undo_now() : 0;
    if (genopt_get(t, id) == value && id != GO_KIND)
        return 0;
    switch (id) {
    case GO_KIND:
        if (!value) {
            if (!t->gen.kind) return 0;
            t->gen.kind = 0;
            break;
        }
        if (t->gen.kind == value) return 0;
        gen_keep(t);
        gen_fill(t, (unsigned)value);
        break;
    case GO_MARB: t->gen.marbles = (uint8_t)value; break;
    case GO_OCT: t->gen.oct = (int8_t)value; break;
    case GO_FLW: t->gen.follow = (uint8_t)value; break;
    case GO_CHRD: song.gen.journey = (uint8_t)value; break;
    case GO_BARS: song.gen.bars = (uint8_t)value; break;
    case GO_LOCK: song.gen.lock = (uint8_t)value; break;
    }
    return 1;
}
#endif
