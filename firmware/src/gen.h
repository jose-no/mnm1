/* SPDX-License-Identifier: GPL-3.0-only
 * GEN: a phrase generator with live variation, for MNM-1.
 *
 * Ported from Mutable Dreams 1.2 by Kid Bunka / Christoph Link (app/dsp/dreams.cpp: generate_sequence,
 * the Marbles-inspired remembered variation, chord_offset, md_quantize; GPL-3.0-only), with gratitude to
 * Émilie Gillet and Mutable Instruments. Adapted to MNM-1's steps: Mutable Dreams' velocity becomes ACC on the
 * beat, its chance PROB, its per-step gate is not carried (MNM-1's GATE is the track's).
 *
 * Three parts:
 *  - gen_fill (main loop): writes a track's 64 steps from the pattern's seed: SPARSE, ARP, PULSE or DRIFT, at
 *    KEY + OCT, snapped to the track's SCALE (TRANSPOSE page). They are ordinary steps afterwards: edit, lock,
 *    ratchet them as any other.
 *  - gen_step (the audio ISR, seq.c step_fire): plays a step through MARBLES (an off-beat note becomes the root, a
 *    third, fourth or fifth; the same choice for the same seed, position and 2-bar span, so a phrase remembers
 *    its variations; LOCK repeats the first span's forever) and the CHORD journey (the chord's root degree in the
 *    track's scale, every BARS bars). The stored steps never change while playing.
 *  - mutate / undo (main loop): REC on a GEN page draws a new seed and writes the selected track's phrase again;
 *    UNDO puts back the steps, seed and settings from before the last write (one level). The main loop's calls
 *    run with the audio ISR off (fm1_irq_off does not nest: the caller's guard).
 */
#ifndef MNM_GEN_H
#define MNM_GEN_H
#include "seq_transpose.h"

static uint32_t gen_xs(uint32_t *r)               /* xorshift32 (Mutable Dreams' random_word) */
{
    uint32_t x = *r ? *r : GEN_SEED_DEFAULT;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *r = x;
    return x;
}

/* the 1/24 beats of a step of track t (1/32: 3 .. 4 BAR: 384); 0 for a division without a whole number */
static uint32_t gen_step24(const track_t *t)
{
    uint32_t den, num = div_num_den((uint32_t)t->p[P_SDIV], &den);
    return den ? num * 24u / den : 0u;
}

/* the semitones above KEY of the journey's chord root: degrees of the track's scale (Mutable Dreams' chord_offset;
 * --- FIX MAJ MAJ5 take the major degrees, MIN MIN5 the minor ones, as its pentatonic scales do) */
static int gen_chord_offset(const track_t *t, uint32_t pos24)
{
    static const uint8_t DEGREE[5][4] = {{0,0,0,0},{0,3,0,4},{0,5,3,4},{0,6,3,0},{0,2,5,3}};
    static const uint8_t INTERVAL[5][7] = {
        {0,2,4,5,7,9,11}, {0,2,3,5,7,8,10}, {0,2,3,5,7,9,10}, {0,1,3,5,7,8,10}, {0,2,4,6,8,10,12}};
    static const uint8_t OF_SCALE[SEQ_NSCALES] = {0,0,0,1,2,3,1,0,4};   /* --- FIX MAJ MIN DOR PHR MIN5 MAJ5 WHL */
    uint32_t j = song.gen.journey, bars = song.gen.bars ? song.gen.bars : 1u, sc = t->seq.scale_key >> 4;
    if (!j || j > 4u)
        return 0;
    return INTERVAL[OF_SCALE[sc < SEQ_NSCALES ? sc : 0u]][DEGREE[j][(pos24 / (96u * bars)) & 3u]];
}

/* the phrase's root: KEY + 12 x OCT, as a step's NOTE (C4 = 0) */
static int gen_base(const track_t *t)
{
    return (int)((t->seq.scale_key & 15u) % 12u) + 12 * t->gen.oct;
}

/* a step's NOTE snapped to the track's scale around its KEY (none for --- and FIX); ties go down, as seq_note */
static int gen_snap(const track_t *t, int nt)
{
    uint32_t sc = t->seq.scale_key >> 4, key = (t->seq.scale_key & 15u) % 12u, mask;
    int d;
    nt = clamp(nt, NT_MIN, NT_MAX);
    if (sc < 2u || sc >= SEQ_NSCALES)
        return nt;
    mask = SEQ_SCALE_MASK[sc];
    for (d = 0; d < 12; d++) {
        int lo = nt - d, hi = nt + d;
        if (lo >= NT_MIN && (mask >> ((uint32_t)(lo + 120 - (int)key) % 12u) & 1u))
            return lo;
        if (hi <= NT_MAX && (mask >> ((uint32_t)(hi + 120 - (int)key) % 12u) & 1u))
            return hi;
    }
    return nt;
}

/* ---- the audio ISR: the step as it plays */
static step_t gen_play[NTRK];

static const step_t *gen_step(track_t *t, const step_t *s)
{
    static const uint8_t MARBLE[4] = {0, 3, 5, 7};
    uint32_t k = (uint32_t)(t - trk), pos24, idx = t->seq_idx;
    int journey = song.gen.journey && t->gen.follow;
    step_t *o;
    if (!s->on || (!t->gen.marbles && !journey))
        return s;
    o = &gen_play[k % NTRK];
    *o = *s;
    pos24 = t->seq_cnt * gen_step24(t);                /* the track's time since PLAY, 1/24 beats */
    if (t->gen.marbles && idx % 4u) {                 /* an off-beat: its remembered choice */
        uint32_t span = song.gen.lock ? 0u : pos24 / 192u;   /* (2 bars) */
        uint32_t r = gen_seed(&song.gen) ^ (k * 0x9e3779b9u) ^ ((idx % 16u + 1u) * 0x85ebca6bu) ^ (span * 0xc2b2ae35u);
        uint32_t c = gen_xs(&r);
        if (c % 100u < t->gen.marbles)
            o->nt = (int8_t)gen_snap(t, gen_base(t) + MARBLE[(c >> 12) & 3u]);
    }
    if (journey)
        o->nt = (int8_t)clamp(o->nt + gen_chord_offset(t, pos24), NT_MIN, NT_MAX);
    return o;
}

/* ---- the main loop: writing phrases, mutate, undo */
static struct {
    step_t step[NSTEP];                            /* the track's steps before the last write */
    gen_options_t opt;
    uint32_t seed;
    uint32_t wrote;                                /* gen_hash of the steps that write left (0: none since) */
    uint8_t track, valid;
} gen_undo;

static uint32_t gen_hash(const step_t *s)          /* FNV-1a of a track's steps */
{
    const uint8_t *b = (const uint8_t *)s;
    uint32_t h = 2166136261u, n = NSTEP * sizeof *s;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h | 1u;
}

/* keeps what undo puts back: the steps as they are, unless they are still exactly what GEN wrote last on this
 * track (turning through the phrases keeps the steps from before the first) */
static void gen_keep(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk);
    if (gen_undo.valid && gen_undo.track == k && gen_undo.wrote && gen_undo.wrote == gen_hash(t->step))
        return;
    memcpy(gen_undo.step, t->step, sizeof gen_undo.step);
    gen_undo.opt = t->gen;
    gen_undo.seed = gen_seed(&song.gen);
    gen_undo.track = (uint8_t)k;
    gen_undo.valid = 1;
}

/* Mutable Dreams' generate_sequence: kind 1 SPARSE (beats, a fifth between), 2 ARP (eighths through a minor-seventh
 * figure; MNM-1: the seed picks where in the figure it starts, the default seed Mutable Dreams' start), 3 PULSE (two of every three sixteenths), 4 DRIFT (beats and 45 % of the rest, a random figure note,
 * PROB 80 %). Every step is written; LEN chooses how many play. Call with the audio ISR off. */
static void gen_fill(track_t *t, uint32_t kind)
{
    static const int8_t MELODY[8] = {0, 7, 12, 3, 5, 7, 10, 12};
    uint32_t k = (uint32_t)(t - trk), i, g;
    uint32_t r = gen_seed(&song.gen) ^ (k + 1u) * 0x9e3779b9u;
    int base = gen_base(t);
    uint32_t rot = (gen_seed(&song.gen) ^ GEN_SEED_DEFAULT) % 8u;
    if (kind < 1u || kind > 4u)
        return;
    g = kind - 1u;
    for (i = 0; i < NSTEP; i++) {
        uint32_t roll = gen_xs(&r);
        int on = i % 4u == 0u || (g == 1u ? i % 2u == 0u : g == 2u ? i % 3u != 1u : g == 3u ? roll % 100u < 45u : 0);
        int off = g == 0u ? (i % 8u ? 7 : 0) : MELODY[g == 1u ? (i / 2u + rot) % 8u : (roll >> 12) % 8u];
        step_t *s = &t->step[i];
        *s = (step_t){0};
        s->on = on ? 1u : 0u;
        s->acc = i % 4u == 0u;
        s->cond = (uint8_t)(g == 3u ? 17u : 0u);   /* PROB 80 % (cond 1..20 = 0..95 %) */
        s->nt = (int8_t)gen_snap(t, base + off);
    }
    t->gen.kind = (uint8_t)kind;
    gen_undo.wrote = gen_hash(t->step);
}

/* REC on a GEN page: a new seed; the selected track's phrase written again from it (main loop, ISR off). Returns 1
 * when the track's notes changed, 0 when only the seed did (no generator chosen, or SPARSE: the same at any seed;
 * MARBLES still varies with it). */
static int gen_mutate(track_t *t)
{
    uint32_t r = gen_seed(&song.gen), before = gen_hash(t->step);
    gen_keep(t);
    gen_xs(&r);
    gen_xs(&r);
    gen_seed_set(&song.gen, r);
    if (t->gen.kind)
        gen_fill(t, t->gen.kind);
    return gen_hash(t->step) != before;
}

/* UNDO: the steps, seed and settings from before the last write; 0: nothing to undo */
static int gen_undo_now(void)
{
    track_t *t;
    if (!gen_undo.valid || gen_undo.track >= NTRK)
        return 0;
    t = &trk[gen_undo.track];
    memcpy(t->step, gen_undo.step, sizeof t->step);
    t->gen = gen_undo.opt;
    gen_seed_set(&song.gen, gen_undo.seed);
    gen_undo.valid = 0;
    gen_undo.wrote = 0;
    return 1;
}
#endif
