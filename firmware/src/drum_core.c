/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Drum machine fork: 2026 DEADACTIVE */
/* The 8 tracks: power-on kit, hits (voice choice, choke groups), and the per-track render fx.c mixes
 * (track_render). A cut voice is not dropped: its last sample decays in dtail. */

static uint32_t dvage;                                   /* hit counter: voice ages, noise seeds */
static uint8_t route_owner[NTRK];
static void route_release(track_t *t);
static uint8_t safe_start;                            /* SEQ held at power-on: no drum audio (drum_boot_init) */

static uint8_t arp_self;
#include "controller_mod.h"
#include "multi_runtime.h"
static multi_runtime_t multi __attribute__((section(".pool")));
static uint8_t multi_owner[NTRK],multi_dispatching;


/* power-on kit (a new project, TOOLS INIT ALL too): MNM-1's six tracks, routed as a kit of synth and FX machines: a GND kick; a
 * metallic FM+ snare ("CLING", after SOPHIE's snares: inharmonic FM, short, high-passed) heard only through
 * the FLANGER on the next track (NEIB; its OUT OFF), a short delay with high feedback ringing as a comb; an SWSAW bass;
 * an SWPLS arp on bus CD, dry, and into the REVERB on T6 (IN CD, OUT AB: a send and return). Model and MIDI note */
static const uint8_t KIT_DEF[NTRK][2] = {
    {DM_MMGND, 36}, {DM_MMFM, 38}, {DM_FXFLG, 39}, {DM_MMSAW, 40}, {DM_MMPLS, 41}, {DM_FXREV, 42},
};
/* .. and their sounds: the machine's 8 knobs; AMP ATK HOLD DEC REL PORT; FILTER BASE WDTH HPQ LPQ ATK DEC BOFS WOFS
 * (filter.c: BASE 0 two octaves under the note); LEN; ARP MODE SPEED RANGE CHORD; OUT IN GATE (P_OUT P_IN P_FXG);
 * the delay DSND DTIM DFB (fx.c track_delay: the arp's a ping-pong in dotted eighths) */
typedef struct { int8_t k[8]; uint8_t amp[5]; int8_t flt[8]; uint8_t len, arp[4], out, in, fxg; int8_t dly[3]; } kit_snd_t;
static const kit_snd_t KIT_SND[NTRK] = {
    {{-24, 0, 100, 80, 34, 0, 0, 0}, {0, 8, 52, 20, 0}, {0, 127, 0, 0, 0, 40, 0, 0}, 64, {0, 2, 1, 0}, BUS_AB, IN_NEIB, 0,
     {0, 48, 32}},
    {{7, 13, 70, 40, 38, 110, 23, 1}, {0, 0, 58, 30, 0}, {22, 127, 0, 0, 0, 40, 0, 0}, 64, {0, 2, 1, 0}, BUS_OFF, IN_NEIB,
     0, {0, 48, 32}},
    {{10, 3, 12, 90, 61, 0, 0, 64}, {0, 127, 127, 40, 0}, {30, 127, 0, 0, 0, 40, 0, 0}, 96, {0, 2, 1, 0}, BUS_AB, IN_NEIB,
     0, {0, 48, 32}},
    {{-12, 14, 2, 70, 0, 110, 100, -1}, {0, 127, 127, 24, 0}, {0, 29, 0, 72, 0, 46, 25, 0}, 80, {0, 2, 1, 0}, BUS_AB,
     IN_NEIB, 0, {0, 48, 32}},
    {{0, 64, 60, 30, 0, 0, 20, 50}, {0, 127, 127, 40, 0}, {0, 38, 0, 30, 0, 40, 17, 0}, 127, {1, 2, 2, 6}, BUS_CD, IN_NEIB,
     0, {28, 48, 44}},
    {{90, 50, 127, 127, 20, 100, 0, 64}, {0, 127, 127, 40, 0}, {0, 127, 0, 0, 0, 40, 0, 0}, 96, {0, 2, 1, 0}, BUS_AB, IN_CD,
     0, {0, 48, 32}},
};
static void kit_sound(track_t *t, const kit_snd_t *s)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = s->k[i];
    t->p[P_AATK] = s->amp[0];
    t->p[P_AHOLD] = s->amp[1];
    t->p[P_ADEC] = s->amp[2];
    t->p[P_AREL] = s->amp[3];
    t->p[P_PORT] = s->amp[4];
    t->p[P_FCUT] = s->flt[0];
    t->p[P_FWDTH] = s->flt[1];
    t->p[P_FRESO] = s->flt[2];
    t->p[P_FLPQ] = s->flt[3];
    t->p[P_FATK] = s->flt[4];
    t->p[P_FDEC] = s->flt[5];
    t->p[P_FENV] = s->flt[6];
    t->p[P_FWOFS] = s->flt[7];
    t->p[P_DLY] = s->dly[0];
    t->p[P_DTIM] = s->dly[1];
    t->p[P_DFB] = s->dly[2];
    t->p[P_NLEN] = s->len;
    t->p[P_ARPM] = s->arp[0];
    t->p[P_ARPS] = s->arp[1];
    t->p[P_ARPR] = s->arp[2];
    t->p[P_ARPC] = s->arp[3];
    t->p[P_OUT] = s->out;
    t->p[P_IN] = s->in;
    t->p[P_FXG] = s->fxg;
}

static const dmodel_t *trk_model(const track_t *t) { return &DMODELS[(uint32_t)t->p[P_MODEL] % NMODELS]; }
/* a gated model's voice with its gate open (ACID: x[2] bit 0; a synth machine: its own) */
static int dv_gate_open(const track_t *t, const dvoice_t *v)
{
    const dmodel_t *m = &DMODELS[t->model % NMODELS];
    return v->active && m->gate && (m->mm >= 2u ? mmen_state[mmen_index(t)].gate : m->mm ? v->ms.mm.gate : (v->x[2] & 1));
}

/* a track's swing: its own + the global one, capped at 100 (upstream #31: 200 made 1.8 / 0.2 steps); seq.c, slicer.c */
#define SWING_MAX 100
static inline int32_t track_swing(const track_t *t)
{
    int32_t sw = t->p[P_SSWING] + song.g[G_SWING];
    return sw < 0 ? 0 : sw > SWING_MAX ? SWING_MAX : sw;
}

static void dv_cut(track_t *t, dvoice_t *v)
{
    if (v->active)
        t->dtail += v->last;
    v->active = 0;
    v->last = 0;
}

static void drum_cut(track_t *t)                         /* every voice of t, with the declick tail */
{
    route_release(t);
    uint32_t i;
    for (i = 0; i < NDV; i++)
        dv_cut(t, &t->v[i]);
    t->gate_n = 0;                                       /* ACID: no gate left to close, nothing slides on */
    t->slide_in = 0;
}

static void model_follow(track_t *t)                     /* the model changed: the old voices stop */
{
    if (t->model != (uint8_t)t->p[P_MODEL]) {
        drum_cut(t);
        mmen_ready[mmen_index(t)] = 0;
        if (mmen_index(t) < NTRK) fxs[mmen_index(t)].model = 255;
        t->model = (uint8_t)t->p[P_MODEL];
    }
}

#ifndef DRUM_MAXV
#define DRUM_MAXV 8       /* voices sounding at once over all tracks, model + layer (user decision: realistic use fits the
                           * 1566 reference; the extreme case, ~2388, relies on the device's overload shedding, M1-B;
                           * a heavy model's voice counts 2 (M1-C, user decision) */
#endif

/* what a sounding voice counts toward DRUM_MAXV: its model's weight */
static uint32_t dv_weight(const track_t *t, const dvoice_t *v)
{
    uint32_t w = DMODELS[t->model % NMODELS].weight;
    (void)v;
    return w ? w : 1u;
}

/* the voice to go first of any track: *ot its track; returns the sounding weight. A gated model's
 * voice (ACID) with its gate open goes last: a TIE or a slide keeps the voice and its age, so a held note would be
 * the oldest and any drum hit would cut it; it is taken only when nothing else sounds. Then a LOW track's voice before
 * a HIGH one's (P_PRIO), then the oldest */
static uint32_t dv_oldest(track_t **ot, dvoice_t **ov)
{
    uint32_t i, k, n = 0, key, best = 0;
    *ot = 0;
    *ov = 0;
    for (i = 0; i < NTRK; i++)
        for (k = 0; k < NDV; k++) {
            dvoice_t *v = &trk[i].v[k];
            if (!v->active)
                continue;
            n += dv_weight(&trk[i], v);
            key = (uint32_t)dv_gate_open(&trk[i], v) * 2u + !trk[i].p[P_PRIO];
            if (!*ov || key < best || (key == best && v->age < (*ov)->age)) {
                *ov = v;
                *ot = &trk[i];
                best = key;
            }
        }
    return n;
}

/* a voice of weight w is about to start: sounding voices stop (declick tail; dv_oldest's order: a held ACID note last,
 * a LOW track's first, the oldest) until it fits the cap */
static void dv_make_room(uint32_t w)
{
    track_t *ot;
    dvoice_t *ov;
    while (dv_oldest(&ot, &ov) + w > DRUM_MAXV && ov)
        dv_cut(ot, ov);
}

/* audio overload (audio.c, > 95 % of a half): one voice stops (dv_oldest's order: a held ACID note last, a LOW
 * track's first, the oldest) */
static void drum_shed(void)
{
    track_t *ot;
    dvoice_t *ov;
    if (dv_oldest(&ot, &ov) && ov)
        dv_cut(ot, ov);
}

static dvoice_t *dv_alloc(track_t *t, dvoice_t *pool, uint32_t nv, uint32_t w)   /* free, else the oldest */
{
    uint32_t i;
    dvoice_t *v = &pool[0];
    for (i = 0; i < nv; i++) {
        if (!pool[i].active) {
            dv_make_room(w);
            return &pool[i];
        }
        if (pool[i].age < v->age)
            v = &pool[i];
    }
    dv_cut(t, v);
    return v;
}

static void dv_init(dvoice_t *v, uint32_t vel)
{
    memset(v, 0, sizeof *v);
    v->active = 1;
    v->vel = (uint8_t)(vel > 127u ? 127u : vel < 1u ? 1u : vel);
    v->age = ++dvage;
    v->rng = (int32_t)(dvage * 2654435761u) | 1;
}

/* silent: muted, or another track soloed and not this one (SOLO: the SMC-Mixer; P_MUTE itself unchanged) */
static int trk_silent(const track_t *t)
{
    return t->p[P_MUTE] || (song.solo && !((song.solo >> (uint32_t)(t - trk)) & 1u));
}

static void arp_note(track_t *t, int glide);             /* arp.c */

/* one hit of track t at velocity vel (1..127), playing note nt (an offset from C4) */
static void drum_play_event(track_t *t, uint32_t vel, int32_t nt,unsigned flags)
{
    const dmodel_t *m = trk_model(t);
    uint32_t i, nv = m->voices < NDV ? m->voices : NDV;
    dvoice_t *v;
    if(flags&ST_NOTEOFF){
        t->rat_n=0;t->slide_in=0;t->gate_n=0;
        if(m->gate&&t->v[0].active)m->gate(t,&t->v[0],0);
        return;
    }
    t->nt=(flags&ST_NOPITCH)?t->trig_nt:(int8_t)clamp(nt,NT_MIN,NT_MAX);
    t->trig_nt=t->nt;
    if (safe_start)
        return;
    if(!(flags&ST_NOAMP))arp_note(t, 0);                                     /* ARP: a note of its own starts a run */
    if(!(flags&ST_NOLFO))lfo_hit(t);                                     /* HIT LFOs restart, the trigger reads their values */
    if (trk_silent(t) && (uint32_t)(t - trk) != comp_ghost_src())   /* a muted COMP source still plays (GHOST KEEP /
                                                                      * HIDE, fx.c) */
        return;
    if(!(flags&ST_NOFLT))trk_filter_hit(t, vel);                             /* FILTER: the hit's sweep (filter.c) */
    if(flags&ST_NOAMP){
        if(mmen_is_model(t->model))mmen_trigger_mask(t,&t->v[0],vel,flags);
        else if(!(flags&ST_NOPITCH)&&m->glide&&t->v[0].active){
            uint32_t age=t->v[0].t;
            m->glide(t,&t->v[0]);t->v[0].t=age;
        }
        return;
    }
    if (m->fx && !t->p[P_FXG])
        return;                                         /* an FX machine at GATE ON: always passing, its notes its
                                                         * filter's sweep and its pitch only (mm_fx.c) */
    if (t->p[P_CHOKE])
        for (i = 0; i < NTRK; i++)
            if (&trk[i] != t && trk[i].p[P_CHOKE] == t->p[P_CHOKE])
                drum_cut(&trk[i]);
    model_follow(t);
    if ((uint32_t)t->p[P_MODEL] % NMODELS != DM_SMPL || smpl_playable(t)) {   /* nothing to play: no voice */
        v = dv_alloc(t, t->v, nv, m->weight ? m->weight : 1u);
        dv_init(v, vel);
        if(flags&&mmen_is_model(t->model))mmen_trigger_mask(t,v,vel,flags);
        else m->trigger(t, v);
    }
}
static uint32_t hit_gate_len(const track_t *t);
static unsigned route_mask(unsigned source)
{
    unsigned mask=0;
    while(source<NTRK&&!(mask&(1u<<source))){
        mask|=1u<<source;unsigned next=trk[source].perf.route_flags&7u;
        if(!next||next>NTRK)break;source=next-1;
    }
    return mask;
}
/* Last-note ownership prevents an old source release from cutting a newer note. */
static void route_release(track_t *t)
{
    unsigned source=ctlmod_index(t);if(source==NTRK)return;
    for(unsigned k=0;k<NTRK;k++)if(k!=source&&route_owner[k]==source+1){
        track_t *target=&trk[k];const dmodel_t *m=trk_model(target);
        route_owner[k]=0;target->gate_n=0;target->rat_n=0;target->slide_in=0;
        if(m->gate&&target->v[0].active)m->gate(target,&target->v[0],0);
    }
}
static void drum_play_one(track_t *t,uint32_t vel,int32_t nt,unsigned flags)
{
    unsigned index=ctlmod_index(t);
    if(index<NTRK&&!multi_dispatching&&!arp_self)multi_owner[index]=0;
    int temporary=ctlmod_note_begin(t,vel,(flags&ST_NOPITCH)?t->trig_nt:nt,flags);
    drum_play_event(t,vel,nt,flags);
    ctlmod_note_end(t,temporary);
}
static unsigned drum_play_routes(unsigned roots,uint32_t vel,int32_t nt,unsigned flags)
{
    unsigned visited=0;
    for(unsigned source=0;source<NTRK;source++)if(roots&(1u<<source)){
        unsigned mask=route_mask(source)&~visited;visited|=mask;
        for(unsigned k=0;k<NTRK;k++)if(mask&(1u<<k)){
            track_t *target=&trk[k];
            drum_play_one(target,vel,nt,flags);route_owner[k]=(uint8_t)(source+1);
            if(k!=source&&!(flags&(ST_NOAMP|ST_NOTEOFF))&&trk_model(target)->gate){
                target->gate_n=trk_model(&trk[source])->gate?0:hit_gate_len(target);
                target->slide_in=0;
            }
        }
    }
    return visited;
}
static void drum_play_masked(track_t *t,uint32_t vel,int32_t nt,unsigned flags)
{
    unsigned k=ctlmod_index(t);
    if(flags&ST_NOTEOFF){route_release(t);drum_play_one(t,vel,nt,flags);return;}
    if(k==NTRK){drum_play_one(t,vel,nt,flags);return;}
    if(!(t->perf.route_flags&7u)){
        drum_play_one(t,vel,nt,flags);route_owner[k]=(uint8_t)(k+1);return;
    }
    drum_play_routes(1u<<k,vel,nt,flags);
}
static void drum_forward_note(track_t *t,uint32_t vel,int32_t nt)
{
    unsigned source=ctlmod_index(t);if(source==NTRK)return;
    unsigned mask=route_mask(source)&~(1u<<source);
    for(unsigned k=0;k<NTRK;k++)if(mask&(1u<<k)){
        drum_play_one(&trk[k],vel,nt,0);route_owner[k]=(uint8_t)(source+1);
        trk[k].gate_n=0;trk[k].slide_in=0;
    }
}
static void drum_play(track_t *t,uint32_t vel,int32_t nt){drum_play_masked(t,vel,nt,0);}

static uint32_t hit_gate_len(const track_t *t);          /* seq.c */

/* MIDI, the keys, Grids: C4. A gated model's (ACID) note no step drives ends as a step's: its gate closes after
 * GATE of the track's step (seq.c seq_tick counts it, stopped too) */
static void drum_hit(track_t *t, uint32_t vel)
{
    drum_play(t, vel, 0);
    if (trk_model(t)->gate) {
        t->gate_n = hit_gate_len(t);
        t->slide_in = 0;
    }
}

/* the engine view's rendered hit (ui_draw.c vg_step), on the UI thread: t's model, one voice (a two-voice model's one
 * hit; no sample layer, no FX, no RESON, no LFO), struck on the scratch track vg_trk (dm_dsp.c) with t's knobs at C4,
 * velocity vel. Nothing live is touched: no voice from the pool, no voice cap, no choke, no age or seed from dvage (age
 * 1 and its seed, as the first hit after power-on: the same knobs, the same sound), the metal bank and SKIN's body the
 * scratch's own. A SAMPLE with no sample: no voice */
static void vg_hit(const track_t *t, uint32_t vel)
{
    dvoice_t *v = &vg_trk.v[0];
    memset(&vg_trk, 0, sizeof vg_trk);
    memcpy(vg_trk.p, t->p, sizeof vg_trk.p);
    vg_trk.model = (uint8_t)((uint32_t)t->p[P_MODEL] % NMODELS);
    memset(vg_metal_ph, 0, sizeof vg_metal_ph);
    memset(&vg_memb, 0, sizeof vg_memb);
    if (vg_trk.model == DM_SMPL && !smpl_playable(&vg_trk))
        return;
    v->active = 1;
    v->vel = (uint8_t)vel;
    v->age = 1;
    v->rng = (int32_t)2654435761u | 1;
    DMODELS[vg_trk.model].trigger(&vg_trk, v);
}

#define VG_MM_GATE (FS * 3u / 10u)                       /* the engine view's note of a synth machine: 0.3 s */
/* the next n (<= CTL) samples of the rendered hit into out (cleared first); 0 once its voice has ended */
static uint32_t vg_render(int32_t *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        out[i] = 0;
    if (!vg_trk.v[0].active)
        return 0;
    if (DMODELS[vg_trk.model].mm && vg_trk.v[0].t >= VG_MM_GATE && dv_gate_open(&vg_trk, &vg_trk.v[0]))
        DMODELS[vg_trk.model].gate(&vg_trk, &vg_trk.v[0], 0);           /* a synth machine's note: held 0.3 s, then its RELEASE */
    DMODELS[vg_trk.model].render(&vg_trk, &vg_trk.v[0], out, n);
    return 1;
}

/* track t's voices and tail into out (cleared first); returns non-zero while anything sounds */
static uint32_t track_render(track_t *t, int32_t *out, uint32_t n)
{
    const dmodel_t *m = trk_model(t);
    uint32_t i, nr = 0;
    for (i = 0; i < n; i++)
        out[i] = 0;
    if (t->wow)
        t->wow = mulq16(t->wow, ACID_WOW);              /* ACID's accent smoother falls every block, sounding or not */
    model_follow(t);
    if (m->fx && !t->p[P_FXG]) {                        /* an FX machine at GATE ON: its voice always there, but */
        if (trk_silent(t) || safe_start)                /* muted */
            dv_cut(t, &t->v[0]);
        else if (!t->v[0].active && fx_in_on) {         /* (asleep until its input sounds: mm_fx.c FX_SLEEP) */
            dv_init(&t->v[0], 127);
            m->trigger(t, &t->v[0]);
        }
    }
    for (i = 0; i < NDV; i++)
        if (t->v[i].active) {
            m->render(t, &t->v[i], out, n);
            nr++;
        }
    if(!nr&&mmen_is_model(t->model))mmen_idle_filter(t,n);
    if (t->dtail) {
        for (i = 0; i < n && t->dtail; i++) {
            int32_t d = t->dtail >> 4;
            out[i] += t->dtail;
            t->dtail -= d ? d : (t->dtail > 0 ? 1 : -1);
        }
        nr++;
    }
    return nr;
}

static void drum_block_begin(void) { dblock++; }

/* model mi with its default sound on t: the voice's first preset (presets.c; else the knobs' defaults), the track's
 * reference on it, not edited. CHOKE is the track's own: a MODEL change never touches it (spec 2026-10-19) */
static void drum_set_model(track_t *t, uint32_t mi)
{
    const dmodel_t *m = &DMODELS[mi % NMODELS];
    uint32_t i, id = preset_first_of(mi % NMODELS);
    preset_t pr;
    if (model_refused(t, mi % NMODELS))
        return;
    drum_cut(t);
    if (t->p[P_MODEL] >= MM_MODEL_FIRST && mi < MM_MODEL_FIRST)
        mmen_legacy_defaults(t);
    mmen_ready[mmen_index(t)] = 0;
    if (mmen_index(t) < NTRK) fxs[mmen_index(t)].model = 255;
    t->p[P_MODEL] = (int16_t)(mi % NMODELS);
    t->model = (uint8_t)t->p[P_MODEL];
    if (m->mm >= 2u)
        mm_controls_init(t, (uint32_t)(t - trk) < NTRK ? (uint32_t)(t - trk) : 0, m->mm - 2u);
    if (id != PRE_NONE && preset_get(id, &pr)) {
        preset_apply(t, &pr, id);
        return;
    }
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = m->edit[i].def;
    t->pre = (uint16_t)PRE_NONE;
    t->pre_ed = 0;
}

/* (ACID_MAXTRK: presets.c) */

static uint32_t acid_tracks(const track_t *except)   /* tracks with MODEL ACID, other than except */
{
    uint32_t i, n = 0;
    for (i = 0; i < NTRK; i++)
        n += &trk[i] != except && (uint32_t)trk[i].p[P_MODEL] % NMODELS == DM_ACID;
    return n;
}

static void drum_tracks_init(void)
{
    uint32_t i, k;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.act[G_KSLOT-G_COUNT]=1;
    song.seq=(pattern_options_t){0};
    gen_pattern_default(&song.gen);
    multi_options_default(&song.multi);
    ctlmod_reset();
    memset(&multi,0,sizeof multi);memset(multi_owner,0,sizeof multi_owner);
    song.multi_pitch16=0;multi_dispatching=0;
    memset(route_owner,0,sizeof route_owner);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        memset(t, 0, sizeof *t);
        seq_options_default(&t->seq);
        gen_options_default(&t->gen);
        for (i = 0; i < P_COUNT; i++)
            if (i < P_E0 || i > P_E7)
                t->p[i] = TP[i].def;
        drum_set_model(t, KIT_DEF[k][0]);
        kit_sound(t, &KIT_SND[k]);
        t->p[P_NOTE] = KIT_DEF[k][1];
        if (KIT_DEF[k][0] == DM_SMPL)
            t->p[P_E5] = KIT_DEF[k][1];             /* the GM kit's sound for that note */
    }
    song.sel = 0;
    song.master_q12 = 2048;
}
