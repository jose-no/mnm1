/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Drum machine fork: 2026 DEADACTIVE */
/* Four reset-persistent RAM slots, backed by flash. MNM9 adds GEN (gen.h); MNM8 extends song rows; MNM6 compacted parameters to
 * bytes and adds sequencer options. MNM3/4/5/6/7 flash records compact in place in
 * storage's scratch buffer; old .noinit layouts are ignored across upgrades. */
#define PROJ_MAGIC 0x394D4E4Du                 /* "MNM9": GEN */
#define PROJ_V8_MAGIC 0x384D4E4Du              /* "MNM8": extended song rows */
#define PROJ_V7_MAGIC 0x374D4E4Du
#define PROJ_V6_MAGIC 0x364D4E4Du
#define PROJ_V5_MAGIC 0x354D4E4Du
#define PROJ_V4_MAGIC 0x344D4E4Du
#define PROJ_V3_MAGIC 0x334D4E4Du              /* "MNM3" */
#define PROJ_V3_PARAMS 80u
#define PROJ_V3_GLOBALS 41u
static int expanded_pattern_name(uint32_t slot,char *b);
static void expanded_pattern_save(uint32_t slot);
static int project_save_error;
static uint32_t expanded_chain_prepare(void);
static int expanded_pattern_load(uint32_t slot,int irq);
static void expanded_pattern_rename(uint32_t slot,const char *name);
static void expanded_boot(void);
static void expanded_reset(void);
static int expanded_save_sound(uint32_t slot,const track_t *t);
static int expanded_delete_sound(uint32_t slot);
static int seq_options_valid(const seq_options_t *s);
static int pattern_options_valid(const pattern_options_t *s);
static int perf_options_valid(const perf_options_t *s);
static int multi_options_valid(const multi_options_t *s);
typedef struct {
    uint8_t p[P_COUNT];
    step_t step[NSTEP];
} proj_trk_t;
static const param_desc_t *proj_desc(unsigned model,unsigned id)
{
    if(id>=P_E0&&id<=P_E7)return &DMODELS[model].edit[id-P_E0];
    if(mmen_is_model(model)&&mm_control_desc(id))return mm_control_desc(id);
    return &TP[id];
}
/* Stored bytes use two's complement only for a signed parameter descriptor. */
static int16_t proj_value(const proj_trk_t *s,unsigned id)
{
    unsigned m=s->p[P_MODEL]<NMODELS?s->p[P_MODEL]:0;
    return proj_desc(m,id)->min<0?(int8_t)s->p[id]:s->p[id];
}
static uint8_t proj_byte(unsigned model,unsigned id,int value)
{
    const param_desc_t *d=proj_desc(model,id);
    return (uint8_t)clamp(value,d->min,d->max);
}
typedef struct {
    uint32_t magic, size;
    int16_t g[G_COUNT];
    uint8_t sel, rsv[3];
    proj_trk_t t[NTRK];
    seq_options_t seq[NTRK];
    pattern_options_t options;
    motion_store_t motion;                      /* FDR8: the motion (motion.c; MNM-1 0.5: the locks, 240) */
    chain_config_t song;                        /* FDR9: the song (song.c) */
    char name[NAME_LEN];                        /* FDR9: the name (NAME_SET, 0-padded; "" = none) */
    uint16_t pre[NTRK];                         /* FDRD: each track's preset: a factory one's uid, 0x8000 | a user
                                                 * slot, PRE_NONE none (presets.c preset_rec_enc: not the id) */
    uint8_t pre_ed[NTRK];                       /* FDRD: .. edited since (0 / 1) */
    perf_options_t perf[NTRK];
    multi_options_t multi;
    gen_options_t gen[NTRK];                    /* MNM9: GEN, each track's (gen.h) */
    gen_pattern_t gen_pat;                      /* MNM9: GEN, the seed and the chord journey */
    uint32_t sum;
} project_t;
/* MNM8 is MNM9 up to GEN: its sum sat where GEN starts (4-aligned) */
#define PROJ_V8_SIZE (((__builtin_offsetof(project_t,gen)+3u)&~3u)+4u)
_Static_assert(PROJ_V8_SIZE==3648u,"MNM8 layout changed");
/* Frozen MNM6 prefix: future live parameter/option counts must not change it. */
typedef struct {uint8_t p[97];step_t step[64];} mnm_proj_v6_trk_t;
typedef struct {
    uint32_t magic,size;int16_t g[45];uint8_t sel,rsv[3];
    mnm_proj_v6_trk_t t[6];uint8_t seq[6][33],options[4];
    motion_store_t motion;chain_config_v1_t song;char name[12];
    uint16_t pre[6];uint8_t pre_ed[6];uint32_t sum;
} mnm_project_v6_t;
_Static_assert(sizeof(mnm_project_v6_t)==3456,"MNM6 layout changed");
_Static_assert(__builtin_offsetof(project_t,motion)==__builtin_offsetof(mnm_project_v6_t,motion),"MNM6 prefix moved");

/* Fixed old layout: do not use P_COUNT here when adding parameters. */
typedef struct {
    int16_t p[PROJ_V3_PARAMS];
    step_t step[64];
} mnm_proj_v3_trk_t;
typedef struct {
    uint32_t magic, size;
    int16_t g[PROJ_V3_GLOBALS];
    uint8_t sel, rsv[3];
    mnm_proj_v3_trk_t t[6];
    motion_store_t motion;
    chain_config_v1_t song;
    char name[NAME_LEN];
    uint16_t pre[NTRK];
    uint8_t pre_ed[NTRK];
    uint32_t sum;
} mnm_project_v3_t;
/* MNM4 and MNM5 share the fixed 97-parameter, 45-global layout. */
typedef struct {int16_t p[97];step_t step[64];} mnm_proj_v5_trk_t;
typedef struct {
    uint32_t magic,size;int16_t g[45];uint8_t sel,rsv[3];
    mnm_proj_v5_trk_t t[6];motion_store_t motion;chain_config_v1_t song;
    char name[12];uint16_t pre[6];uint8_t pre_ed[6];uint32_t sum;
} mnm_project_v5_t;
/* Frozen MNM7: its performance suffix is shifted only after validation. */
typedef struct {
    uint32_t magic,size;int16_t g[45];uint8_t sel,rsv[3];
    mnm_proj_v6_trk_t t[6];uint8_t seq[6][33],options[4];
    motion_store_t motion;chain_config_v1_t song;char name[12];
    uint16_t pre[6];uint8_t pre_ed[6],perf[6][9],multi[10];uint32_t sum;
} mnm_project_v7_t;
_Static_assert(sizeof(mnm_project_v7_t)==3520,"MNM7 layout changed");
_Static_assert(__builtin_offsetof(project_t,name)==__builtin_offsetof(mnm_project_v7_t,name)+128,"song expansion changed");
_Static_assert(sizeof(mnm_project_v5_t)==3836,"MNM4/5 layout changed");
_Static_assert(sizeof(mnm_project_v3_t) == 3624u, "MNM3 layout changed");
_Static_assert(P_NSTORED >= PROJ_V3_PARAMS, "MNM4 parameter block shrank");
typedef char project_size[sizeof(project_t) <= 3840u ? 1 : -1];   /* (MNM-1: 6 tracks, room left) */
typedef struct {step_t step[NTRK][NSTEP];motion_store_t motion;} expanded_chain_slot_t;
static union {
    project_t projects[4];
    expanded_chain_slot_t chain[6];
} proj_memory __attribute__((section(".noinit")));
_Static_assert(sizeof proj_memory<=15696u, "project/chain cache exceeds RAM budget");
#define proj_slot proj_memory.projects
static expanded_chain_slot_t expanded_chain_extra[2];

static uint32_t proj_hash(const void *p, uint32_t n)   /* FNV-1a over n bytes */
{
    const uint8_t *b = p;
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}
static uint32_t proj_sum(const project_t *p) { return proj_hash(p, sizeof *p - 4u); }
static int proj_ok(const project_t *q) { return q->magic == PROJ_MAGIC && q->size == sizeof *q && q->sum == proj_sum(q); }

/* Forward copying compacts overlapping legacy fields without a project-sized stack. */
static void proj_compact_copy(void *dst,const void *src,unsigned n)
{
    uint8_t *d=dst;const uint8_t *s=src;
    while(n--)*d++=*s++;
}
static int proj_options_valid(const project_t *q)
{
    if(!chain_valid(&q->song)||!pattern_options_valid(&q->options)||!multi_options_valid(&q->multi)||
       !gen_pattern_valid(&q->gen_pat))return 0;
    for(unsigned k=0;k<NTRK;k++)
        if(!seq_options_valid(&q->seq[k])||!perf_options_valid(&q->perf[k])||!gen_options_valid(&q->gen[k]))return 0;
    return 1;
}
/* every project older than MNM9: GEN off (its phrases are plain steps already) */
static void proj_gen_default(project_t *q)
{
    for(unsigned k=0;k<NTRK;k++)gen_options_default(&q->gen[k]);
    gen_pattern_default(&q->gen_pat);
}
static void proj_upgrade_performance(mnm_project_v7_t *q)
{
    memset((uint8_t *)q+__builtin_offsetof(mnm_project_v7_t,perf),0,
           sizeof *q-__builtin_offsetof(mnm_project_v7_t,perf));
    multi_options_t defaults;multi_options_default(&defaults);
    memcpy(q->multi,&defaults,sizeof defaults);
}
static int proj_expand_song(project_t *q)
{
    /* Preserve every suffix byte before the new options overwrite its old home. */
    uint8_t *dst=(uint8_t *)&q->name;
    const uint8_t *src=(const uint8_t *)q+__builtin_offsetof(mnm_project_v7_t,name);
    unsigned n=__builtin_offsetof(mnm_project_v7_t,sum)-__builtin_offsetof(mnm_project_v7_t,name);
    while(n){--n;dst[n]=src[n];}
    memset(q->song.options,0,sizeof q->song.options);
    memset(q->song.rsv,0,sizeof q->song.rsv);
    /* Legacy loads discarded a damaged arrangement while retaining valid notes/sounds. */
    if(!chain_valid(&q->song))memset(&q->song,0,sizeof q->song);
    proj_gen_default(q);
    q->magic=PROJ_MAGIC;q->size=sizeof *q;q->sum=proj_sum(q);
    return proj_options_valid(q);
}
static int proj_decode(project_t *q,uint32_t n)
{
    if(n<8)return 0;
    const mnm_project_v3_t *v3=(const void *)q;
    const mnm_project_v5_t *v5=(const void *)q;
    mnm_project_v7_t *v7=(void *)q;
    unsigned old3=q->magic==PROJ_V3_MAGIC,old5=q->magic==PROJ_V5_MAGIC;
    if(n==sizeof *q&&proj_ok(q)){
        if(!chain_valid(&q->song)){
            memset(&q->song,0,sizeof q->song);q->sum=proj_sum(q);
        }
        return proj_options_valid(q);
    }
    unsigned oldsize;
    switch(q->magic){
    case PROJ_V3_MAGIC:oldsize=sizeof *v3;break;
    case PROJ_V4_MAGIC:case PROJ_V5_MAGIC:oldsize=sizeof *v5;break;
    case PROJ_V6_MAGIC:oldsize=sizeof(mnm_project_v6_t);break;
    case PROJ_V7_MAGIC:oldsize=sizeof *v7;break;
    case PROJ_V8_MAGIC:oldsize=PROJ_V8_SIZE;break;
    default:return 0;
    }
    if(n!=oldsize||q->size!=oldsize)return 0;
    uint32_t sum;memcpy(&sum,(const uint8_t *)q+oldsize-4,4);
    if(sum!=proj_hash(q,oldsize-4))return 0;
    if(q->magic==PROJ_V8_MAGIC){
        if(!chain_valid(&q->song))memset(&q->song,0,sizeof q->song);   /* (as an MNM8 read did) */
        proj_gen_default(q);
        q->magic=PROJ_MAGIC;q->size=sizeof *q;q->sum=proj_sum(q);
        return proj_options_valid(q);
    }
    if(q->magic==PROJ_V7_MAGIC)return proj_expand_song(q);
    if(q->magic==PROJ_V6_MAGIC){proj_upgrade_performance(v7);return proj_expand_song(q);}
    uint8_t sel=old3?v3->sel:v5->sel;
    /* First normalize into the old compact prefix, whose tail has not grown. */
    for(unsigned k=0;k<NTRK;k++) {
        const int16_t *p=old3?v3->t[k].p:v5->t[k].p;
        const step_t *steps=old3?v3->t[k].step:v5->t[k].step;
        uint8_t values[P_COUNT];unsigned model=(unsigned)clamp(p[P_MODEL],0,NMODELS-1);
        for(unsigned i=0;i<P_COUNT;i++)values[i]=proj_byte(model,i,
            i<(old3?80u:97u)?p[i]:TP[i].def);
        memcpy(v7->t[k].p,values,sizeof values);
        proj_compact_copy(v7->t[k].step,steps,sizeof v7->t[k].step);
        if(!old5)for(unsigned i=0;i<NSTEP;i++)v7->t[k].step[i].rsv=0;
    }
    const void *tail=old3?(const void *)&v3->motion:(const void *)&v5->motion;
    unsigned tailsize=__builtin_offsetof(mnm_project_v6_t,sum)-__builtin_offsetof(mnm_project_v6_t,motion);
    proj_compact_copy(&v7->motion,tail,tailsize);
    if(old3)for(unsigned i=PROJ_V3_GLOBALS;i<G_COUNT;i++)v7->g[i]=GP[i].def;
    v7->sel=sel;memset(v7->rsv,0,sizeof v7->rsv);
    seq_options_t defaults;seq_options_default(&defaults);
    for(unsigned k=0;k<NTRK;k++)memcpy(v7->seq[k],&defaults,sizeof defaults);
    memset(v7->options,0,sizeof v7->options);
    proj_upgrade_performance(v7);
    return proj_expand_song(q);
}

#if FELUCCA_FLASH
static void proj_fetch(uint32_t slot)
{
    project_t *q = &proj_slot[slot & 3u];
    /* st_buf is aligned and already owned by the synchronous storage reader.
     * An old record is larger than a compact slot: never read it into that slot. */
    int n = st_load(OBJ_PROJECT0 + (slot & 3u), st_buf, sizeof st_buf);
    if (n < 0 || !proj_decode((project_t *)(void *)st_buf, (uint32_t)n))q->magic=0;
    else memcpy(q,st_buf,sizeof *q);
}
#endif

/* the sequencer runs (or starts this block): a flash erase silences the audio and stalls it, so no saving now */
static int transport_busy(void) { return song.playing || transport_req == 1u; }

/* 12 stored name bytes -> d (NAME_LEN + 1): up to the first 0; a byte outside NAME_SET: no name */
static void proj_name_get(char *d, const char *s)
{
    uint32_t i;
    name_set(d, "");
    for (i = 0; i < NAME_LEN && s[i]; i++) {
        if (!name_char_ok(s[i])) {
            name_set(d, "");
            return;
        }
        d[i] = s[i];
    }
}

static void project_save(uint32_t slot)
{
    project_save_error=0;
    if(full_user.get){expanded_pattern_save(slot);return;}
    if(slot>=4){ui_message("EMPTY SLOT");return;}
    if (transport_busy()) {                            /* (upstream 1.0) only while stopped */
        ui_message("STOP TO SAVE");
        return;
    }
    project_t *p = &proj_slot[slot & 3u];
    uint32_t i;
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    for (i = 0; i < G_COUNT; i++)
        p->g[i] = song.g[i];
    p->sel = song.sel;
    for (i = 0; i < NTRK; i++) {
        for(unsigned j=0;j<P_COUNT;j++)p->t[i].p[j]=proj_byte((unsigned)trk[i].p[P_MODEL],j,trk[i].p[j]);
        p->seq[i]=trk[i].seq;
        p->gen[i]=trk[i].gen;
        p->perf[i]=trk[i].perf;
        memcpy(p->t[i].step, trk[i].step, sizeof trk[i].step);
        p->pre[i] = (uint16_t)preset_rec_enc(trk[i].pre);   /* (stable: a uid / a user slot, presets.c) */
        p->pre_ed[i] = trk[i].pre_ed;
    }
    p->options=song.seq;
    p->gen_pat=song.gen;
    p->multi=song.multi;
    p->motion = mo.s;                                  /* (stopped: the base is in the knobs) */
    p->song = chain.cfg;
    memcpy(p->name, chain.name, NAME_LEN);
    p->sum = proj_sum(p);
    chain.from = (uint8_t)((slot & 3u) + 1u);
#if FELUCCA_FLASH
    if (flash_ok) {
        if ((project_save_error=st_save(OBJ_PROJECT0 + (slot & 3u), p, sizeof *p))!=0) {
            ui_message("SAVE ERROR");
            return;
        }
        lastp.set = 1;                                  /* the last project: power-on loads it (spec 2026-10-11 §6) */
        lastp.slot = (uint8_t)(slot & 3u);
        settings_save();                                /* (stopped: now; unchanged: nothing written) */
        project_save_error=settings_save_error;
        ui_message(settings_save_error?"SAVED; BOOT ERROR":"SAVED");
        return;
    }
#endif
    ui_message("SAVED (RAM)");
}

/* a stored preset reference (decoded: presets.c preset_rec_dec) of a track with model m: none, or a factory preset of
 * another voice (a uid retouched by another firmware), is none; a user slot's is kept as it is (its slot may hold
 * another voice by now: shown as none, presets.c preset_of) */
static uint32_t preset_ref(uint32_t id, uint32_t m)
{
    if (id >= PRE_NID || (id < PRE_NFACT && PRE_FACTORY[id].model != m))
        return PRE_NONE;
    return id;
}

/* slot -> the live project; 1 loaded. irq: the main loop's load (the audio ISR runs: off while copying); 0 the
 * power-on one (drum_boot_init: before audio_init and fm1_irq_enable_all, so no fm1_irq_on; no EMPTY SLOT message) */
static int proj_load(uint32_t slot, int irq)
{
    if(full_user.get)return expanded_pattern_load(slot,irq);
    if(slot>=4)return 0;
    project_t *p = &proj_slot[slot & 3u];
    uint32_t i, k;
    if (chain_busy()) {                                 /* a song reads the slots */
        ui_message("STOP TO LOAD");
        return 0;
    }
#if FELUCCA_FLASH
    if (flash_ok && !proj_ok(p))
        proj_fetch(slot);
#endif
    if (!proj_ok(p)) {
        if (irq)
            ui_message("EMPTY SLOT");
        return 0;
    }
    if(!proj_decode(p,sizeof *p))return 0;
    transport_req = 2;
    if (irq)
        fm1_irq_off();                                  /* the audio ISR must not see half a project */
    multi_stop();ctlmod_reset();
    song.seq=p->options;
    song.gen=p->gen_pat;
    song.multi=p->multi;
    mmen_reset_all();
    mmen_lfo_reset();
    mmarp_reset();
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_LOAD && i != G_SAVE)
            song.g[i] = (int16_t)(i == G_CLOCK && (p->g[i] < 0 || p->g[i] > GP[i].max)
                                      ? 0 : clamp(p->g[i], GP[i].min, GP[i].max));   /* CLK BLE without BLE: INT */
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const proj_trk_t *s = &p->t[k];
        uint32_t m = (uint32_t)clamp(proj_value(s,P_MODEL), 0, NMODELS - 1);
        drum_cut(t);
        for (i = 0; i < P_COUNT; i++) {                 /* every value back inside its range */
            const param_desc_t *d = i >= P_E0 && i <= P_E7 ? &DMODELS[m].edit[i - P_E0] : &TP[i];
            if (mmen_is_model(m) && mm_control_desc(i)) d = mm_control_desc(i);
            t->p[i] = (int16_t)clamp(proj_value(s,i), d->min, d->max);
        }
        t->seq=p->seq[k];
        t->gen=p->gen[k];
        t->perf=p->perf[k];
        t->p[P_MODEL] = (int16_t)m;
        t->model = (uint8_t)m;
        t->pre = (uint16_t)preset_ref(preset_rec_dec(p->pre[k]), m);
        t->pre_ed = p->pre_ed[k] ? 1u : 0u;
        t->key_nt = 0;                                  /* no last KEYS note carried over (spec §2) */
        for (i = 0; i < NSTEP; i++) {                   /* garbage cannot index a table or divide by zero */
            const step_t *q = &s->step[i];
            t->step[i].on = q->on ? 1u : 0u;
            t->step[i].acc = q->acc ? 1u : 0u;
            t->step[i].cond = (uint8_t)(q->cond < COND_MAX ? q->cond : COND_MAX);
            t->step[i].rat = (uint8_t)(q->rat < 3u ? q->rat : 3u);
            t->step[i].slide = q->slide;
            t->step[i].tie = q->tie;
            t->step[i].pslide = q->pslide;
            t->step[i].nt = (int8_t)clamp(q->nt, NT_MIN, NT_MAX);
            t->step[i].rsv = q->rsv&ST_MASK;
        }
    }
    for (k = 0, i = 0; k < NTRK; k++)                   /* ACID on 2 tracks at most: the first two keep it, the
                                                         * others get their power-on kit model (with its sound) */
        if ((uint32_t)trk[k].p[P_MODEL] == DM_ACID && ++i > ACID_MAXTRK) {
            int16_t note = trk[k].p[P_NOTE];
            drum_set_model(&trk[k], KIT_DEF[k][0]);
            trk[k].p[P_NOTE] = note;
        }
    if (motion_valid(&p->motion))                       /* the motion: a broken block is dropped, the rest loads */
        mo.s = p->motion;
    else
        memset(&mo.s, 0, sizeof mo.s);
    motion_forget();                                    /* the new knobs are the base: the stop puts nothing back */
    if (chain_valid(&p->song))                          /* the song and the name: broken ones dropped */
        chain.cfg = p->song;
    else
        memset(&chain.cfg, 0, sizeof chain.cfg);
    proj_name_get(chain.name, p->name);
    chain.from = (uint8_t)((slot & 3u) + 1u);
    song.sel = (uint8_t)(p->sel < NTRK ? p->sel : 0u);
    if (irq)
        fm1_irq_on();
    ui.force = 1;
    ui_message("LOADED");
    return 1;
}
static void project_load(uint32_t slot) { (void)proj_load(slot, 1); }

/* power-on (drum_boot_init, not in safe start): the last project saved to flash loads as a LOAD does, stopped, its
 * slot PROJECT's SLOT. Empty or unreadable: confirm 5, LOAD FAILED / START NEW PROJECT? (ui_input.c) over the power-on
 * kit */
static void project_boot(void)
{
    if (!lastp.set)
        return;
    if (proj_load(lastp.slot, 0)) {
        song.g[G_SLOT] = (int16_t)(lastp.slot + 1u);
        return;
    }
    ui.confirm = 5;
    ui.confirm_trk = lastp.slot;
    ui.force = 1;
}

/* settings + learned panel table: one flash object (format unchanged from Felucca, so a calibrated
 * panel survives the change of firmware). The flash copy wins at boot. */
typedef struct {
    uint32_t magic, palette, lowcut, zoom;          /* palette: bits 0..7 COLOR, bit 8 the BT AUTOSTART (2026-10-12) */
    panel_t panel;
} persist_t;
#define PERSIST_MAGIC 0x50455232u                  /* "PER2" */
#if FELUCCA_FLASH
static persist_t persist_saved;
#endif

/* MIDI LEARN's map (midi_learn.c ml_map), its own flash object OBJ_UPRESET0 (storage.c's user-preset object, unused
 * by Hortator; 0xDC000..0xDDFFF): "HLRN", version 2, ML_N (32) entries {num, trk, code, src} (code: a P_* id + 1 or
 * an action, ML_A_*). Version 1 (16 entries, CCs and parameters only, 72 B) loads into it. Anything else there (none,
 * Felucca's user presets, another version) reads as nothing learned; a broken entry is dropped (spec 2026-10-13 §5,
 * §7) */
#define ML_MAGIC 0x4E524C48u                       /* "HLRN" */
#define ML_N1 16u                                  /* version 1's entries */
typedef struct {
    uint32_t magic;
    uint16_t ver, n;
    ml_ent_t e[ML_N];
} ml_rec_t;
typedef char ml_rec_size[sizeof(ml_rec_t) == 136u && 8u + 4u * ML_N1 == 72u ? 1 : -1];
#if FELUCCA_FLASH
static ml_rec_t ml_saved;                           /* what the flash holds (a save of the same map writes nothing) */
static void ml_rec_make(ml_rec_t *r)
{
    memset(r, 0, sizeof *r);
    r->magic = ML_MAGIC;
    r->ver = 2;
    r->n = ML_N;
    memcpy(r->e, ml_map, sizeof r->e);
}
#endif
/* persist_boot (IRQs off, the map cleared): the map from flash (version 2, or 1), its broken entries dropped: a source
 * or a code this build does not know or cannot learn, a note on a parameter or a macro, a track past 8 (an action:
 * not 0), a source or a code already in an earlier entry */
static void learn_load(void)
{
#if FELUCCA_FLASH
    memset(&ml_saved, 0, sizeof ml_saved);
    {
        ml_rec_t r;
        uint32_t i, j, n = 0, len;
        int got;
        memset(&r, 0, sizeof r);
        got = flash_ok ? st_load(OBJ_UPRESET0, &r, sizeof r) : -1;
        if (got < 0 || r.magic != ML_MAGIC)
            return;
        len = (uint32_t)got;
        if (r.ver == 1u && r.n == ML_N1 && len == 8u + 4u * ML_N1) {
            for (i = 0; i < ML_N1; i++)                 /* (version 1: src was a reserved 0, CC) */
                if (r.e[i].src)
                    r.e[i].code = 0;
            r.ver = 2;                                  /* (as a version 2 of the same map: no write for it) */
            r.n = ML_N;
        } else if (r.ver != 2u || r.n != ML_N || len != sizeof r) {
            return;
        }
        ml_saved = r;
        for (i = 0; i < ML_N; i++) {
            const ml_ent_t *e = &r.e[i];
            uint32_t s = (uint32_t)(e->num | e->src << 8);
            int ok = e->code && e->src <= ML_NOTE && ml_src_ok(s) && ml_code_ok(e->trk, e->code) &&
                     (e->src != ML_NOTE || ml_btn(e->code));
            for (j = 0; ok && j < n; j++)
                ok = (uint32_t)(ml_map[j].num | ml_map[j].src << 8) != s &&
                     !(ml_map[j].code == e->code && ml_map[j].trk == e->trk);
            if (ok)
                ml_map[n++] = *e;
        }
    }
#endif
}
/* after a learn / clear (main loop): the map saved when it changed; while playing (or PLAY pending) once stopped
 * (settings_poll): a write erases a sector with the audio silenced */
static void learn_save(void)
{
    if (transport_busy()) {
        ui.ml.save = 1;
        return;
    }
    ui.ml.save = 0;
#if FELUCCA_FLASH
    {
        static ml_rec_t r;                          /* (static: 136 B off the main path's deepest stack) */
        if (!flash_ok)
            return;
        ml_rec_make(&r);
        if (!memcmp(&r, &ml_saved, sizeof r))
            return;
        if (st_save(OBJ_UPRESET0, &r, sizeof r) == 0)
            ml_saved = r;
    }
#endif
}

/* the user presets (presets.c upr): their own flash object OBJ_UPRESET0 + 1 (storage.c's second user-preset object,
 * unused before; 0xDE000..0xDFFFF; MIDI LEARN's map keeps OBJ_UPRESET0): "UPR1", 128 slots {used, model, k[8]} (1284 B).
 * Another record there (none, Felucca's presets, another version) reads as no user preset; a slot that is not used 1
 * or names no model is empty, its knobs are put inside the model's ranges. A storage.c save is two copies and a commit
 * record: a power cut keeps the last record whole */
static void upr_write(void)                         /* the bank to flash: now when stopped, else at the next STOP */
{
    if (transport_busy()) {
        upr_pending = 1;
        return;
    }
    upr_pending = 0;
#if FELUCCA_FLASH
    if (!flash_ok)
        return;
    upr.magic = UPR_MAGIC;
    if (st_save(OBJ_UPRESET0 + 1u, &upr, sizeof upr))
        ui_message("SAVE ERROR");
#endif
}
/* t's voice into user slot (0..127, over what it holds): in the list at once, in flash as upr_write. 0, -1 a bad slot.
 * The knobs as set: while MOTION plays the track, its base (motion.c), not the value motion moved a knob to. The track
 * is not changed (the caller points its reference at PRE_NFACT + slot) */
static int preset_user_save(uint32_t slot, const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk);
    if (slot >= PRE_NUSER)
        return -1;
    if (full_user.get) return expanded_save_sound(slot,t);
    if (t->p[P_MODEL] >= MM_MODEL_FIRST) {
        ui_message("FULL SOUND STORE NEEDED");
        return -1;
    }
    upr_set(slot, (uint32_t)t->p[P_MODEL], k < NTRK && ((mo.base_ok >> k) & 1u) ? mo.base[k] : t->p);
    upr_write();
    return 0;
}
static void preset_user_delete(uint32_t slot)       /* user slot emptied (tracks loaded from it keep their knobs) */
{
    if (full_user.get) {(void)expanded_delete_sound(slot);return;}
    if (slot >= PRE_NUSER || upr.s[slot].used != 1u)
        return;
    memset(&upr.s[slot], 0, sizeof upr.s[slot]);
    upr_write();
}
static void preset_user_boot(void)                  /* persist_boot (after flash_ok): the bank from flash */
{
    memset(&upr, 0, sizeof upr);
    upr_pending = 0;
#if FELUCCA_FLASH
    {
        uint32_t i, k;
        if (!flash_ok || st_load(OBJ_UPRESET0 + 1u, &upr, sizeof upr) != (int)sizeof upr || upr.magic != UPR_MAGIC) {
            memset(&upr, 0, sizeof upr);
            return;
        }
        for (i = 0; i < PRE_NUSER; i++) {
            upr_slot_t *u = &upr.s[i];
            if (u->used != 1u || u->model >= NMODELS) {
                memset(u, 0, sizeof *u);
                continue;
            }
            for (k = 0; k < 8u; k++)
                u->k[k] = (int8_t)clamp(u->k[k], DMODELS[u->model].edit[k].min, DMODELS[u->model].edit[k].max);
        }
    }
#endif
}
static void preset_user_tick(void)                  /* the main loop (settings_poll): a change left for the STOP */
{
    if (upr_pending && !transport_busy())
        upr_write();
}

/* the stored lowcut word's bits 0..7 (bits 8..31: btset.bt_hi): 0..1 SPEAKER EQ, 2..4 the last project (2 set, 3..4
 * its slot), 5..6 METRO (spec 2026-10-11 §9), 7 BACKLIGHT not OFF (§12; its level: the palette word's bits 9..11,
 * fixes 2026-10-21 item 9, settings_save). An older firmware reads SPEAKER EQ FLAT from a record with any of 2..7 set */
static uint32_t lc_pack(void)
{
    return (settings.lowcut & 3u) |
           (lastp.set ? 1u << 2 | (uint32_t)(lastp.slot & 3u) << 3 : 0u) | (uint32_t)(fx_metro & 3u) << 5 |
           (uint32_t)(backlight != BL_OFF) << 7;
}
static void lc_unpack(uint32_t lc)
{
    settings.lowcut = lc & 3u;                       /* (3: settings_init makes it FLAT) */
    lastp.set = (uint8_t)((lc >> 2) & 1u);
    lastp.slot = (uint8_t)((lc >> 3) & 3u);
    fx_metro = (uint8_t)((lc >> 5) & 3u);
    backlight = (lc >> 7) & 1u;                      /* (ON: DIM LO; the level from the palette word, persist_boot) */
}

static void persist_boot(void)                    /* before settings_init / panel_init */
{
    memset(&btset, 0, sizeof btset);                  /* no device, MODE AUTO, AUTOSTART OFF unless the flash record
                                                       * has them */
    memset(&lastp, 0, sizeof lastp);                  /* no last project, unless the flash record has one */
    fx_metro = 0;                                     /* METRO OFF, as well */
    backlight = 0;                                    /* BACKLIGHT OFF, as well */
    memset(ml_map, 0, sizeof ml_map);                 /* MIDI LEARN: nothing learned, as well */
    memset(&upr, 0, sizeof upr);                      /* no user preset, as well (presets.c) */
    upr_pending = 0;
    expanded_reset();
#if FELUCCA_FLASH
    persist_t p;
    uint32_t f = irq_save();
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;
    irq_restore(f);
    if (!flash_ok)
        return;
    fl_plain_window_init();
    if (st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && p.magic == PERSIST_MAGIC) {
        settings.magic = SETTINGS_MAGIC;
        settings.palette = p.palette & 0xFFu;       /* bits 0..7 the palette; bit 8 AUTOSTART (BLE: bt_page.c) */
        btset.bt_auto = (p.palette >> 8) & 1u;
        lc_unpack(p.lowcut);
        lastp.slot |= (uint8_t)(((p.palette >> 12) & 1u) << 2);
        if (((p.palette >> 9) & 7u) - 1u < BL_COUNT - 1u && backlight)   /* BACKLIGHT's level, bits 9..11 (item 9; 0:
                                                     * an older record, its ON = DIM LO) */
            backlight = (p.palette >> 9) & 7u;
        settings.zoom = p.zoom & 1u;
        settings.mutebar = (p.zoom >> 1) & 1u;     /* kept in zoom's bit 1: the flash format stays Felucca's */
        settings.accel = !((p.zoom >> 2) & 1u);    /* bit 2 = KNOB ACCEL OFF: older saves read as on */
        settings.usbfix = (p.zoom >> 3) & 1u;      /* bit 3 = USB LEVEL FIXED: older saves read MASTER */
        settings.perfpage = (p.zoom >> 4) & 1u;    /* bit 4 = PERFORM PAGE: older saves read HOLD */
        btset.btmode = (p.zoom >> 5) & 3u;         /* bits 5..6 = BT MODE, 7 its address type, 8..31 its address */
        if (btset.btmode > 2u)                     /* bytes 0..2; lowcut bits 8..31 bytes 3..5 (all 0 = none) */
            btset.btmode = 0;
        btset.bt_type = (p.zoom >> 7) & 1u;
        btset.bt_lo = p.zoom >> 8;
        btset.bt_hi = p.lowcut >> 8;
        if (p.panel.magic == PANEL_MAGIC)
            panel = p.panel;
        persist_saved = p;
    }
    learn_load();                                     /* MIDI LEARN's map (its own object) */
    preset_user_boot();                               /* the user presets (theirs) */
    {
        uint32_t i;
        for (i = 0; i < 4u; i++)
            if (!proj_ok(&proj_slot[i]))
                proj_fetch(i);
    }
    expanded_boot();
    for (uint32_t k=0;k<SMP_USER_SLOTS;k++) smp_user_scan(k);
#endif
}

static int project_used(uint32_t slot) { char b[NAME_LEN+1];return full_user.get?expanded_pattern_name(slot,b):(slot<4&&proj_ok(&proj_slot[slot])); }
static int project_name(uint32_t slot, char *b)   /* slot's name -> b (NAME_LEN + 1, "" none); 0 = an empty slot */
{
    if(full_user.get)return expanded_pattern_name(slot,b);
    name_set(b, "");
    if(slot>=4)return 0;
    const project_t *p = &proj_slot[slot];
    if (!proj_ok(p))
        return 0;
    proj_name_get(b, p->name);
    return 1;
}
static void project_rename(uint32_t slot, const char *name)   /* only the name; the current one too if it is that slot */
{
    if(full_user.get){expanded_pattern_rename(slot,name);return;}
    if(slot>=4){ui_message("EMPTY SLOT");return;}
    project_t *p = &proj_slot[slot];
    char n[NAME_LEN + 1u];
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return;
    }
    if (!proj_ok(p)) {
        ui_message("EMPTY SLOT");
        return;
    }
    name_set(n, name);
    memcpy(p->name, n, NAME_LEN);
    p->sum = proj_sum(p);
    if (chain.from == (slot & 3u) + 1u)
        name_set(chain.name, n);
#if FELUCCA_FLASH
    if (flash_ok) {
        ui_message(st_save(OBJ_PROJECT0 + (slot & 3u), p, sizeof *p) ? "SAVE ERROR" : "RENAMED");
        return;
    }
#endif
    ui_message("RENAMED (RAM)");
}
/* PLAY on SONG (main loop): the rows' slots -> chain.src, armed; the ISR starts it (seq_start). 0 armed, 1 no rows,
 * 2 busy, 3 + s slot s empty. A slot's steps are safe as stored (step_fire bounds PROB and RATCH); its timing is put
 * inside the ranges, a broken motion plays none */
static uint32_t chain_prepare(void)
{
    uint32_t i, k, j, used = 0;
    if (transport_busy() || chain_busy())
        return 2;
    if (!chain.cfg.count || !chain_valid(&chain.cfg))
        return 1;
    if(chain.cfg.rsv[0])return demo_song_prepare();
    if(full_user.get)return expanded_chain_prepare();
    for (i = 0; i < chain.cfg.count; i++) {
        uint32_t s = chain.cfg.row[i].slot;
        if (!project_used(s))
            return 3u + s;
        used |= 1u << s;
    }
    fm1_irq_off();
    for (i = 0; i < 4u; i++)
        if ((used >> i) & 1u) {
            const project_t *p = &proj_slot[i];
            chain_src_t *d = &chain.src[i];
            d->seq=p->seq;d->options=&p->options;
            d->m = motion_valid(&p->motion) ? &p->motion : &MOTION_NONE;
            for (k = 0; k < NTRK; k++) {
                d->step[k] = p->t[k].step;
                d->model[k] = (uint8_t)proj_value(&p->t[k],P_MODEL);
                for (j = 0; j < 4u; j++) {
                    uint32_t id = CHAIN_TIMING[j];
                    d->timing[k][j] = (int16_t)clamp(proj_value(&p->t[k],id), TP[id].min, TP[id].max);
                }
            }
        }
    chain.armed = 1;
    fm1_irq_on();
    transport_req = 1;
    return 0;
}

static void settings_save(void)                    /* asked for while playing: written once stopped (settings_poll) */
{
    settings_save_error=0;
    if (transport_busy()) {
        ui.persist_pending = 1;
        return;
    }
    ui.persist_pending = 0;
#if FELUCCA_FLASH
    persist_t p;
    if (!flash_ok)
        return;
    memset(&p, 0, sizeof p);
    p.magic = PERSIST_MAGIC;
    p.palette = (settings.palette & 0xFFu) | (btset.bt_auto & 1u) << 8 | (backlight & 7u) << 9 | (uint32_t)(lastp.slot & 4u) << 10;   /* an older
                                                     * Hortator reads a palette out of range from bit 8: its display
                                                     * settings reset; 9..11 BACKLIGHT's level (item 9: an older one reads
                                                     * lowcut's bit 7, ON) */
    p.lowcut = lc_pack() | btset.bt_hi << 8;
    p.zoom = (settings.zoom & 1u) | (settings.mutebar & 1u) << 1 | (uint32_t)!settings.accel << 2 | (settings.usbfix & 1u) << 3 |
             (settings.perfpage & 1u) << 4 | (btset.btmode & 3u) << 5 | (btset.bt_type & 1u) << 7 | btset.bt_lo << 8;
    p.panel = panel;
    if (!memcmp(&p, &persist_saved, sizeof p))
        return;
    if ((settings_save_error=st_save(OBJ_SETTINGS, &p, sizeof p)) == 0)
        persist_saved = p;
#endif
}

static void settings_poll(void)                    /* every UI frame: a settings save left for the stop */
{
    if (ui.persist_pending && !transport_busy())
        settings_save();
    if (ui.ml.save && !transport_busy())             /* .. and MIDI LEARN's map */
        learn_save();
    preset_user_tick();                              /* .. and the user presets */
}

#if FELUCCA_FLASH
_Static_assert(sizeof(project_t) <= ST_PAYLOAD_MAX, "project does not fit one flash sector");
#endif
#include "mm_storage_ui.h"
