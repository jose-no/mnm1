/* SPDX-License-Identifier: GPL-3.0-only
 * GEN (firmware/src/gen.h): phrases, scale snapping, MARBLES, the CHORD journey, mutate / undo, the panel options,
 * the pattern (v5) and project (MNM9) records and their upgrades, and the steps as the sequencer plays them. */
#define FELUCCA_FLASH 1
#include <stdint.h>
static uint8_t hflash[0x100000];
#define SMP_USER_XIP(k) ((const uint8_t *)hflash + 0xA0000u + (k) * 0x14000u)
#include "ui_host.h"
#include <assert.h>

static void reset(void)
{
    host_init_live();
    transport_req = 0;
    song.playing = 0;
    gen_pattern_default(&song.gen);
    memset(&gen_undo, 0, sizeof gen_undo);
    for (unsigned k = 0; k < NTRK; k++) {
        trk[k].p[P_SLEN] = 16;
        trk[k].p[P_SDIV] = 2;                       /* 1/16 */
        trk[k].p[P_SRC] = 0;
        trk[k].p[P_ARPM] = 0;
        seq_options_default(&trk[k].seq);
        gen_options_default(&trk[k].gen);
        memset(trk[k].step, 0, sizeof trk[k].step);
    }
}

static int in_scale(const track_t *t, int nt)
{
    unsigned sc = t->seq.scale_key >> 4, key = t->seq.scale_key & 15u;
    return (SEQ_SCALE_MASK[sc] >> ((unsigned)(nt + 120 - (int)key) % 12u)) & 1u;
}

static void phrases(void)
{
    reset();
    track_t *t = &trk[0];
    gen_fill(t, 1);                                  /* SPARSE: the beats, root then fifths */
    for (unsigned i = 0; i < NSTEP; i++) {
        assert(t->step[i].on == (i % 4 == 0));
        assert(t->step[i].acc == (i % 4 == 0));
        assert(t->step[i].nt == (i % 8 ? 7 : 0));
        assert(t->step[i].cond == 0 && !t->step[i].rat && !t->step[i].tie && !t->step[i].slide);
    }
    assert(t->gen.kind == 1);
    gen_fill(t, 2);                                  /* ARP: eighths through the figure (the default seed: from 0) */
    static const int8_t fig[8] = {0, 7, 12, 3, 5, 7, 10, 12};
    for (unsigned i = 0; i < NSTEP; i++) {
        assert(t->step[i].on == (i % 2 == 0));
        assert(t->step[i].nt == fig[(i / 2) % 8]);
    }
    gen_fill(t, 3);                                  /* PULSE: two of every three, and the beats */
    for (unsigned i = 0; i < NSTEP; i++)
        assert(t->step[i].on == (i % 4 == 0 || i % 3 != 1));
    gen_fill(t, 4);                                  /* DRIFT: PROB 80 %, beats always */
    unsigned on = 0;
    for (unsigned i = 0; i < NSTEP; i++) {
        assert(t->step[i].cond == 17);
        if (i % 4 == 0) assert(t->step[i].on);
        on += t->step[i].on;
    }
    assert(on > 16 && on < 64);
    step_t drift[NSTEP];
    memcpy(drift, t->step, sizeof drift);
    gen_fill(t, 4);                                  /* the same seed: the same phrase */
    assert(!memcmp(drift, t->step, sizeof drift));
    gen_fill(&trk[1], 4);                            /* another track: another phrase */
    assert(memcmp(drift, trk[1].step, sizeof drift));
    gen_seed_set(&song.gen, 12345);
    gen_fill(t, 4);                                  /* another seed: another phrase */
    assert(memcmp(drift, t->step, sizeof drift));
    gen_fill(t, 0);                                  /* (0, 5: nothing) */
    gen_fill(t, 5);
    assert(t->gen.kind == 4);

    t->seq.scale_key = (uint8_t)(6u << 4 | 9u);      /* MIN5 on A */
    t->gen.oct = -2;
    for (uint32_t seed = 1; seed < 40; seed++) {
        gen_seed_set(&song.gen, seed * 2654435761u);
        for (unsigned kind = 1; kind <= 4; kind++) {
            gen_fill(t, kind);
            for (unsigned i = 0; i < NSTEP; i++) {
                assert(in_scale(t, t->step[i].nt));
                assert(t->step[i].nt >= 9 - 24 - 1 && t->step[i].nt <= 9 - 24 + 13);
            }
        }
    }
    t->gen.oct = 3;                                  /* the top octave stays inside NOTE's range */
    t->seq.scale_key = (uint8_t)(2u << 4 | 11u);
    gen_fill(t, 2);
    for (unsigned i = 0; i < NSTEP; i++)
        assert(t->step[i].nt >= NT_MIN && t->step[i].nt <= NT_MAX && in_scale(t, t->step[i].nt));
}

static void snapping(void)
{
    reset();
    track_t *t = &trk[0];
    for (unsigned sc = 2; sc < SEQ_NSCALES; sc++)
        for (unsigned key = 0; key < 12; key++) {
            t->seq.scale_key = (uint8_t)(sc << 4 | key);
            for (int nt = NT_MIN; nt <= NT_MAX; nt++) {
                int s = gen_snap(t, nt);
                assert(in_scale(t, s) && s >= nt - 6 && s <= nt + 6);
                if (in_scale(t, nt)) assert(s == nt);
                t->nt = (int8_t)nt;                  /* the player's snap agrees (seq_note, no transpose) */
                t->seq.transpose = 0;
                song.seq.transpose = 0;
                song.multi_pitch16 = 0;
                if (nt + 60 >= 6 && nt + 60 <= 121) assert(seq_note(t) == 60 + s);
            }
        }
    t->seq.scale_key = 0;                            /* --- and FIX: as written */
    assert(gen_snap(t, 1) == 1);
    t->seq.scale_key = 1u << 4;
    assert(gen_snap(t, 1) == 1);
}

static void marbles(void)
{
    reset();
    track_t *t = &trk[0];
    gen_fill(t, 2);
    t->seq.scale_key = 0;                            /* C, ---: the variation is root, m3, 4th, 5th */
    step_t s = t->step[2];
    t->seq_idx = 2;
    t->seq_cnt = 2;
    assert(gen_step(t, &s) == &s);                   /* MARB 0, no journey: the step itself */
    t->gen.marbles = 100;
    unsigned seen = 0;
    for (unsigned idx = 0; idx < 64; idx++) {
        step_t q = t->step[idx];
        q.on = 1;
        t->seq_idx = (uint16_t)idx;
        t->seq_cnt = idx;
        const step_t *o = gen_step(t, &q);
        if (idx % 4 == 0) {                          /* the beat keeps its note */
            assert(o->nt == q.nt);
            continue;
        }
        assert(o != &q && (o->nt == 0 || o->nt == 3 || o->nt == 5 || o->nt == 7));
        seen |= 1u << o->nt;
        assert(o->on == q.on && o->acc == q.acc && o->cond == q.cond);
    }
    assert(seen == (1u | 1u << 3 | 1u << 5 | 1u << 7));
    step_t off = t->step[0];
    off.on = 0;
    t->seq_idx = 1;
    assert(gen_step(t, &off) == &off);               /* a rest stays a rest */

    /* the same position in the same 2 bars: the same note; LOCK: every 2 bars as the first */
    t->gen.marbles = 50;
    unsigned differs = 0;
    for (unsigned lock = 0; lock < 2; lock++) {
        song.gen.lock = (uint8_t)lock;
        for (unsigned idx = 1; idx < 16; idx++) {
            if (idx % 4 == 0) continue;
            step_t q = t->step[idx];
            q.on = 1;
            t->seq_idx = (uint16_t)idx;
            t->seq_cnt = idx;
            int first = gen_step(t, &q)->nt;
            t->seq_cnt = idx;
            assert(gen_step(t, &q)->nt == first);
            for (unsigned span = 1; span < 12; span++) {
                t->seq_cnt = idx + span * 32u;       /* 32 sixteenths: 2 bars later, the same step */
                int later = gen_step(t, &q)->nt;
                if (lock) assert(later == first);
                else differs += later != first;
            }
        }
    }
    assert(differs > 10);
    t->seq.scale_key = (uint8_t)(6u << 4);           /* MIN5: the third snaps into the scale */
    t->gen.marbles = 100;
    for (unsigned idx = 1; idx < 64; idx++) {
        step_t q = t->step[idx];
        q.on = 1;
        t->seq_idx = (uint16_t)idx;
        t->seq_cnt = idx;
        assert(in_scale(t, gen_step(t, &q)->nt) || idx % 4 == 0);
    }
}

static void journey(void)
{
    reset();
    track_t *t = &trk[0];
    step_t q = {0};
    q.on = 1;
    q.nt = 0;
    song.gen.journey = 1;                            /* I IV I V */
    song.gen.bars = 1;
    t->seq_idx = 0;
    t->seq_cnt = 0;
    assert(gen_step(t, &q) == &q);                   /* FLLW off: not this track */
    t->gen.follow = 1;
    static const int major[4] = {0, 5, 0, 7}, minor_vi[4] = {0, 8, 5, 7};
    for (unsigned bar = 0; bar < 8; bar++) {
        t->seq_cnt = bar * 16u + 3u;
        assert(gen_step(t, &q)->nt == major[bar % 4]);
    }
    t->seq.scale_key = 3u << 4;                      /* MIN: I VI IV V in minor */
    song.gen.journey = 2;
    song.gen.bars = 2;
    for (unsigned bar = 0; bar < 16; bar++) {
        t->seq_cnt = bar * 16u;
        assert(gen_step(t, &q)->nt == minor_vi[(bar / 2) % 4]);
    }
    t->p[P_SDIV] = 1;                                /* 1/8: the chord changes every 8 steps (1 bar: BARS 1) */
    song.gen.bars = 1;
    song.gen.journey = 1;
    t->seq.scale_key = 0;
    t->seq_cnt = 8;
    assert(gen_step(t, &q)->nt == 5);
    q.nt = NT_MAX;                                   /* a note at the top stays in range */
    assert(gen_step(t, &q)->nt == NT_MAX);
}

static void mutate_undo(void)
{
    reset();
    track_t *t = &trk[2];
    t->step[3].on = 1;
    t->step[3].nt = -5;
    t->step[3].rat = 2;
    step_t mine[NSTEP];
    memcpy(mine, t->step, sizeof mine);
    uint32_t seed = gen_seed(&song.gen);
    assert(!gen_undo_now());                         /* nothing yet */
    gen_keep(t);
    gen_fill(t, 1);
    gen_keep(t);                                     /* turning through the phrases: still mine underneath */
    gen_fill(t, 2);
    step_t arp[NSTEP];
    memcpy(arp, t->step, sizeof arp);
    assert(gen_mutate(t) == 1);
    assert(gen_seed(&song.gen) != seed && t->gen.kind == 2);
    assert(memcmp(arp, t->step, sizeof arp));        /* ARP: the seed moves the figure's start */
    assert(gen_undo_now());
    assert(!memcmp(mine, t->step, sizeof mine) && gen_seed(&song.gen) == seed && t->gen.kind == 0);
    assert(!gen_undo_now());                         /* one level */

    gen_keep(t);                                     /* an edit after a write: undo goes back to the edit */
    gen_fill(t, 3);
    t->step[1].nt = 11;
    step_t edited[NSTEP];
    memcpy(edited, t->step, sizeof edited);
    gen_keep(t);
    gen_fill(t, 4);
    assert(gen_undo_now() && !memcmp(edited, t->step, sizeof edited));

    trk[5].gen.kind = 0;                             /* SPARSE: the same at any seed */
    gen_fill(&trk[5], 1);
    memcpy(mine, trk[5].step, sizeof mine);
    assert(gen_mutate(&trk[5]) == 0 && !memcmp(mine, trk[5].step, sizeof mine));
    trk[4].gen.kind = 0;                             /* no generator: only the seed */
    seed = gen_seed(&song.gen);
    memcpy(mine, trk[4].step, sizeof mine);
    assert(gen_mutate(&trk[4]) == 0 && gen_seed(&song.gen) != seed && !memcmp(mine, trk[4].step, sizeof mine));
}

static void options_page(void)
{
    reset();
    track_t *t = &trk[1];
    t->step[0].on = 1;
    t->step[0].nt = 4;
    assert(genopt_set(t, GO_KIND, 1) && t->gen.kind == 1 && t->step[8].on && !t->step[1].on);
    assert(genopt_get(t, GO_KIND) == 1 && !genopt_set(t, GO_KIND, 1));
    assert(genopt_set(t, GO_KIND, 0) && t->gen.kind == 0 && t->step[8].on);   /* --- keeps the steps */
    assert(!genopt_set(t, GO_KIND, 0) && !genopt_set(t, GO_KIND, 5));
    assert(genopt_set(t, GO_MARB, 100) && t->gen.marbles == 100 && !genopt_set(t, GO_MARB, 101));
    assert(genopt_set(t, GO_OCT, -3) && t->gen.oct == -3 && !genopt_set(t, GO_OCT, 4));
    assert(genopt_set(t, GO_FLW, 1) && t->gen.follow == 1 && !genopt_set(t, GO_FLW, 1));
    assert(genopt_set(t, GO_CHRD, 4) && song.gen.journey == 4 && !genopt_set(t, GO_CHRD, 5));
    assert(genopt_set(t, GO_BARS, 8) && song.gen.bars == 8 && !genopt_set(t, GO_BARS, 0));
    assert(genopt_set(t, GO_LOCK, 1) && song.gen.lock == 1);
    assert(genopt_get(t, GO_UNDO) == 0 && !genopt_set(t, GO_UNDO, 0));
    assert(genopt_set(t, GO_UNDO, 1));               /* GO: the steps from before GEN */
    assert(t->step[0].on && t->step[0].nt == 4 && !t->step[8].on);
    assert(!genopt_set(t, GO_UNDO, 1));              /* nothing left */
    assert(!genopt_set(t, GO_COUNT, 0) && genopt_get(t, GO_COUNT) == 0);
    for (unsigned id = 0; id < GO_COUNT; id++)       /* every value of every knob inside its own range */
        for (int v = GO_DESC[id].min - 1; v <= GO_DESC[id].max + 1; v++) {
            genopt_set(t, id, v);
            assert(gen_options_valid(&t->gen) && gen_pattern_valid(&song.gen));
        }
    assert(seqopt_set(t, &song.seq, SO_SCALE, 8) && (t->seq.scale_key >> 4) == 8);   /* WHL */
    assert(!seqopt_set(t, &song.seq, SO_SCALE, 9));
    assert(!seqopt_set(t, &song.seq, SO_PSCALE, 4)); /* the pattern's SCALE: as before */
}

static const page_t *seq_page(const char *title)   /* SEQ until the page shows */
{
    for (unsigned k = 0; k < NPAGES && !(!ui.home && str_eq(cur_page()->title, title)); k++) {
        press(B_SEQ);
        ui_frame();
        release_all();
        ui_frame();
    }
    assert(!ui.home && str_eq(cur_page()->title, title) && cur_page()->scope == SC_GENOPT);
    return cur_page();
}

static void knob(uint32_t k, int steps)            /* one detent at a time, a second apart (no acceleration) */
{
    for (int i = 0; i < (steps < 0 ? -steps : steps); i++) {
        host_ticks += 1000u * 1000u * FM1_TICKS_PER_US;
        turn(EN_K1 + k, steps < 0 ? -1 : 1);
        ui_frame();
    }
}

static void tap_btn(uint32_t b)                    /* a short press */
{
    press(b);
    ui_frame();
    release_all();
    ui_frame();
}

static void snap(const char *name)
{
    char path[96];
    for (unsigned i = 0; i < 3; i++)
        ui_frame();
    snprintf(path, sizeof path, "build/ui_shots/gen/%s.ppm", name);
    shot(path);
}

/* the panel: SEQ to GEN, its knobs, REC, GEN CHORD and UNDO, as a player would */
static void panel_use(void)
{
    ui_host_init();
    reset();
    song.sel = 2;
    track_t *t = &trk[2];
    drum_set_model(t, DM_MMSAW);
    t->seq.scale_key = (uint8_t)(3u << 4 | 9u);      /* MIN on A */
    t->step[5].on = 1;
    t->step[5].nt = -3;
    step_t mine[NSTEP];
    memcpy(mine, t->step, sizeof mine);
    seq_page("GEN");
    snap("gen_empty");
    knob(0, 2);                                      /* GEN: ARP */
    assert(t->gen.kind == 2 && t->step[0].on && !t->step[1].on && t->step[2].on);
    for (unsigned i = 0; i < NSTEP; i++) assert(in_scale(t, t->step[i].nt));
    knob(1, 30);                                     /* MARB 30 % */
    assert(t->gen.marbles == 30);
    knob(2, -1);                                     /* OCT -1 */
    assert(t->gen.oct == -1);
    knob(3, 1);                                      /* FLLW */
    assert(t->gen.follow == 1);
    knob(0, 2);                                      /* DRIFT, then REC: a new seed, a new phrase */
    assert(t->gen.kind == 4);
    snap("gen_drift");
    step_t drift[NSTEP];
    memcpy(drift, t->step, sizeof drift);
    uint32_t seed = gen_seed(&song.gen);
    tap_btn(B_REC);
    assert(gen_seed(&song.gen) != seed && memcmp(drift, t->step, sizeof drift));
    assert(!song.rec && !ui.srec);                   /* (REC here arms nothing) */
    seq_page("GEN CHORD");
    knob(0, 2);                                      /* CHRD 1645 */
    knob(1, 2);                                      /* BARS 4 */
    knob(2, 1);                                      /* LOCK */
    assert(song.gen.journey == 2 && song.gen.bars == 4 && song.gen.lock == 1);
    snap("gen_chord");
    knob(3, 1);                                      /* UNDO: GO */
    assert(!memcmp(mine, t->step, sizeof mine) && gen_seed(&song.gen) == GEN_SEED_DEFAULT);
    assert(genopt_get(t, GO_UNDO) == 0);
    knob(3, 1);                                      /* nothing more to undo: the steps stay */
    assert(!memcmp(mine, t->step, sizeof mine));
    seq_page("GEN");                                 /* (back round the SEQ pages) */
    assert(t->gen.kind == 0);                        /* (undo: the settings from before, too) */
}

static void records(void)
{
    static mmr_pattern_t pat, raw;
    reset();
    for (unsigned k = 0; k < NTRK; k++) {
        trk[k].gen = (gen_options_t){(uint8_t)(k % 5), (uint8_t)(k * 17), (int8_t)((int)k - 3), (uint8_t)(k & 1)};
        assert(gen_options_valid(&trk[k].gen));
    }
    song.gen.journey = 3;
    song.gen.bars = 7;
    song.gen.lock = 1;
    gen_seed_set(&song.gen, 0xDEADBEEFu);
    gen_options_t want[NTRK];
    for (unsigned k = 0; k < NTRK; k++) want[k] = trk[k].gen;
    gen_pattern_t wantp = song.gen;
    assert(mmr_pattern_capture(&pat, trk, song.g, &mo.s, &chain.cfg, 0, 0, "GEN"));
    assert(pat.version == 5);
    reset();
    assert(mmr_pattern_apply(trk, song.g, &mo.s, &chain.cfg, &pat));
    for (unsigned k = 0; k < NTRK; k++) assert(!memcmp(&want[k], &trk[k].gen, sizeof want[k]));
    assert(!memcmp(&wantp, &song.gen, sizeof wantp));

    raw = pat;                                       /* bad GEN values: the record is refused */
    raw.gen[2].marbles = 101;
    assert(!mmr_pattern_valid(&raw));
    raw = pat;
    gen_seed_set(&raw.gen_pat, 1);
    raw.gen_pat.seed[0] = 0;
    assert(!mmr_pattern_valid(&raw));
    raw = pat;
    raw.gen_pat.bars = 9;
    assert(!mmr_pattern_valid(&raw));

    raw = pat;                                       /* a v4 record (no GEN): GEN off after the upgrade */
    raw.version = 4;
    memset(raw.gen, 0xEE, sizeof raw.gen + sizeof raw.gen_pat);
    assert(!mmr_pattern_upgrade(&raw, sizeof raw));
    assert(mmr_pattern_upgrade(&raw, MMR_PATTERN_V4_SIZE) && raw.version == 5 && mmr_pattern_valid(&raw));
    gen_options_t off;
    gen_options_default(&off);
    gen_pattern_t offp;
    gen_pattern_default(&offp);
    for (unsigned k = 0; k < NTRK; k++) assert(!memcmp(&off, &raw.gen[k], sizeof off));
    assert(!memcmp(&offp, &raw.gen_pat, sizeof offp));
    assert(!memcmp(raw.step, pat.step, sizeof raw.step) && !memcmp(raw.seq, pat.seq, sizeof raw.seq));
}

static void project(void)
{
    static union { project_t q; uint8_t bytes[3840]; } buf;
    reset();
    flash_ok = 0;
    memset(&chain, 0, sizeof chain);
    trk[0].step[0].on = 1;
    trk[0].step[0].nt = 5;
    trk[3].gen = (gen_options_t){2, 40, -1, 1};
    song.gen.journey = 2;
    gen_seed_set(&song.gen, 777);
    project_save(1);
    project_t *p = &proj_slot[1];
    assert(p->magic == PROJ_MAGIC && p->size == sizeof *p && proj_ok(p));
    assert(sizeof(project_t) <= 3840u);
    reset();
    assert(proj_load(1, 0));
    assert(trk[3].gen.kind == 2 && trk[3].gen.marbles == 40 && trk[3].gen.oct == -1 && trk[3].gen.follow == 1);
    assert(song.gen.journey == 2 && gen_seed(&song.gen) == 777);
    assert(trk[0].step[0].on && trk[0].step[0].nt == 5);

    /* an MNM8 record: everything up to GEN, its sum where GEN starts */
    memset(&buf, 0, sizeof buf);
    memcpy(&buf.q, p, PROJ_V8_SIZE - 4u);
    buf.q.magic = PROJ_V8_MAGIC;
    buf.q.size = PROJ_V8_SIZE;
    uint32_t sum = proj_hash(&buf.q, PROJ_V8_SIZE - 4u);
    memcpy(buf.bytes + PROJ_V8_SIZE - 4u, &sum, 4);
    assert(proj_decode(&buf.q, PROJ_V8_SIZE));
    assert(buf.q.magic == PROJ_MAGIC && proj_ok(&buf.q));
    gen_options_t off;
    gen_options_default(&off);
    for (unsigned k = 0; k < NTRK; k++) assert(!memcmp(&off, &buf.q.gen[k], sizeof off));
    assert(gen_seed(&buf.q.gen_pat) == GEN_SEED_DEFAULT && buf.q.gen_pat.journey == 0 && buf.q.gen_pat.bars == 2);
    assert(!memcmp(buf.q.t, p->t, sizeof p->t) && !memcmp(buf.q.seq, p->seq, sizeof p->seq));
    assert(!memcmp(buf.q.name, p->name, sizeof p->name) && !memcmp(&buf.q.multi, &p->multi, sizeof p->multi));
    sum ^= 1u;                                       /* a damaged MNM8: refused */
    memset(&buf, 0, sizeof buf);
    memcpy(&buf.q, p, PROJ_V8_SIZE - 4u);
    buf.q.magic = PROJ_V8_MAGIC;
    buf.q.size = PROJ_V8_SIZE;
    memcpy(buf.bytes + PROJ_V8_SIZE - 4u, &sum, 4);
    assert(!proj_decode(&buf.q, PROJ_V8_SIZE));
}

/* the sequencer: a synth track's notes as they play, MARBLES and the journey on top of the stored steps */
static void playing(void)
{
    reset();
    track_t *t = &trk[0];
    drum_set_model(t, DM_MMSAW);
    seq_start();
    t->gen.follow = 1;
    song.gen.journey = 1;
    song.gen.bars = 1;
    gen_fill(t, 1);                                  /* SPARSE: 0, 7, 0, 7 on the beats */
    step_t before[NSTEP];
    memcpy(before, t->step, sizeof before);
    static const int chord[4] = {0, 5, 0, 7};
    unsigned fired = 0;
    uint32_t last = t->seq_cnt;
    for (unsigned n = 0; n < 48000u * 4u && fired < 64; n++) {
        seq_tick(t, 1);
        if (t->seq_cnt != last) {
            last = t->seq_cnt;
            if (t->step[t->seq_idx].on) {
                unsigned bar = t->seq_cnt / 16u;
                assert(t->nt == t->step[t->seq_idx].nt + chord[bar % 4]);
                fired++;
            }
        }
    }
    assert(fired >= 8);
    assert(!memcmp(before, t->step, sizeof before));  /* the stored steps never change */
}

int main(void)
{
    phrases();
    snapping();
    marbles();
    journey();
    mutate_undo();
    options_page();
    panel_use();
    records();
    project();
    playing();
    puts("gen_test: all passed");
    return 0;
}
