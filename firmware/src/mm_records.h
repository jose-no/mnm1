/* SPDX-License-Identifier: GPL-3.0-only
 * Version-one pattern, kit and full-sound payloads. Include after params,
 * presets, motion and song. Storage supplies atomic writes and checksums.
 * These converters change parameters/metadata only: callers stop voices and
 * reset engine state under their own IRQ guard before applying a sound/kit.
 * Byte parameters retain two's-complement signed values; the selected model's
 * descriptor determines signedness. Field order keeps uint16 fields aligned. */
#ifndef MNM_RECORDS_H
#define MNM_RECORDS_H
#define MMR_VERSION 2u
#define MMR_PATTERN_VERSION 5u                /* 5: GEN (gen.h) */
#define MMR_PATTERN_OLD_SIZE 2704u
#define MMR_PATTERNS 8u
#define MMR_KITS 8u
#define MMR_SOUNDS 128u
#define MMR_SOUNDS_PER_OBJECT 32u
#define MMR_KITS_PER_OBJECT 2u
static const uint8_t MMR_PATTERN_PARAMS[] = {P_MUTE,P_SLEN,P_SDIV,P_SSWING,P_SRC};
static const uint8_t MMR_KIT_PARAMS[] = {P_CHOKE,P_NOTE,P_DUCK,P_PRIO,P_OUT,P_IN,P_FXG};
#define MMR_PATTERN_N (sizeof MMR_PATTERN_PARAMS)
#define MMR_KIT_N (sizeof MMR_KIT_PARAMS)
typedef struct {
    uint8_t used;
    char name[9];
    uint8_t preset_ref[2];
    uint8_t edited;
    uint8_t p[P_COUNT];
    perf_options_t perf;
} mmr_sound_t;
static unsigned mmr_preset_ref(const mmr_sound_t *s){return s->preset_ref[0]|((unsigned)s->preset_ref[1]<<8);}
static void mmr_preset_set(mmr_sound_t *s,unsigned ref){s->preset_ref[0]=(uint8_t)ref;s->preset_ref[1]=(uint8_t)(ref>>8);}
typedef struct {
    uint8_t used;
    char name[NAME_LEN+1];
    int16_t config[NTRK][MMR_KIT_N];
    mmr_sound_t sound[NTRK];
    multi_options_t multi;
} mmr_kit_t;
typedef struct {
    uint16_t version;
    uint8_t used,kit,selected,reserved;
    char name[NAME_LEN];
    int16_t g[G_COUNT];
    int16_t timing[NTRK][MMR_PATTERN_N];
    step_t step[NTRK][NSTEP];
    motion_store_t motion;
    chain_config_t song;
    seq_options_t seq[NTRK];
    pattern_options_t options;
    gen_options_t gen[NTRK];                       /* v5: GEN */
    gen_pattern_t gen_pat;
} mmr_pattern_t;
/* v4 is v5 up to GEN */
#define MMR_PATTERN_V4_SIZE ((__builtin_offsetof(mmr_pattern_t,gen)+1u)&~1u)
_Static_assert(MMR_PATTERN_V4_SIZE==3034u,"pattern v4 layout changed");
static void mmr_pattern_gen_default(mmr_pattern_t *p)
{
    for(unsigned k=0;k<NTRK;k++)gen_options_default(&p->gen[k]);
    gen_pattern_default(&p->gen_pat);
}
/* Frozen pattern v3 prefix: v1/v2 end immediately after its 36-byte SONG. */
typedef struct {
    uint16_t version;uint8_t used,kit,selected,reserved;char name[12];
    int16_t g[45],timing[6][5];step_t step[6][64];motion_store_t motion;
    chain_config_v1_t song;seq_options_t seq[6];pattern_options_t options;
} mmr_pattern_v3_t;
_Static_assert(sizeof(mmr_pattern_v3_t)==2906,"old pattern layout");
_Static_assert(__builtin_offsetof(mmr_pattern_v3_t,seq)==MMR_PATTERN_OLD_SIZE,"old pattern prefix");
static int mmr_pattern_upgrade(mmr_pattern_t *p,unsigned n)
{
    unsigned version=p->version;
    if(version==MMR_PATTERN_VERSION)return n==sizeof *p;
    if(version==4){
        if(n!=MMR_PATTERN_V4_SIZE)return 0;
        mmr_pattern_gen_default(p);p->version=MMR_PATTERN_VERSION;return 1;
    }
    if(version<1||version>3||n!=(version==3?sizeof(mmr_pattern_v3_t):MMR_PATTERN_OLD_SIZE))return 0;
    if(version==3){
        uint8_t *dst=(void *)p->seq;
        const uint8_t *src=(const uint8_t *)p+__builtin_offsetof(mmr_pattern_v3_t,seq);
        for(unsigned i=sizeof p->seq+sizeof p->options;i-->0;)dst[i]=src[i];
    }else{
        if(version==1)for(unsigned k=0;k<NTRK;k++)for(unsigned i=0;i<NSTEP;i++)p->step[k][i].rsv=0;
        for(unsigned k=0;k<NTRK;k++)seq_options_default(&p->seq[k]);
        p->options=(pattern_options_t){0};
    }
    memset(p->song.options,0,sizeof p->song.options);p->song.rsv[0]=p->song.rsv[1]=0;
    mmr_pattern_gen_default(p);
    p->version=MMR_PATTERN_VERSION;return 1;
}
typedef struct {uint16_t version,reserved;mmr_kit_t kit[MMR_KITS_PER_OBJECT];} mmr_kits_t;
typedef struct {uint16_t version,reserved;mmr_sound_t sound[MMR_SOUNDS_PER_OBJECT];} mmr_sounds_t;
/* Frozen version-one records: convert in scratch, never rewrite on read. */
typedef struct {uint8_t used;char name[9];uint16_t preset_ref;uint8_t edited,p[97];} mmr_sound_v1_t;
typedef struct {uint8_t used;char name[13];int16_t config[6][7];mmr_sound_v1_t sound[6];} mmr_kit_v1_t;
_Static_assert(sizeof(mmr_sound_v1_t)==110 && sizeof(mmr_kit_v1_t)==758,"old sound/kit layout");
_Static_assert(sizeof(mmr_sound_t)==119,"full sound must fit 32 records per object");
static void mmr_sound_upgrade(mmr_sound_t *out,const mmr_sound_v1_t *old)
{
    /* Expanded group records move upward; kit input is a separate local copy. */
    for(unsigned i=110;i-->0;)((uint8_t *)out)[i]=((const uint8_t *)old)[i];
    memset(&out->perf,0,sizeof out->perf);
}
static int mmr_group_upgrade(void *data,unsigned n,int kits)
{
    uint16_t version;memcpy(&version,data,2);
    if(version==MMR_VERSION)return n==(kits?sizeof(mmr_kits_t):sizeof(mmr_sounds_t));
    if(version!=1||n!=(kits?1520u:3524u))return 0;
    uint8_t *bytes=data;
    if(kits){
        mmr_kits_t *out=data;
        for(unsigned i=2;i-->0;){
            mmr_kit_v1_t old;memcpy(&old,bytes+4+i*sizeof old,sizeof old);
            memset(&out->kit[i],0,sizeof out->kit[i]);
            memcpy(&out->kit[i],&old,98);
            for(unsigned k=0;k<6;k++)mmr_sound_upgrade(&out->kit[i].sound[k],&old.sound[k]);
            multi_options_default(&out->kit[i].multi);
        }
    }else{
        mmr_sounds_t *out=data;
        for(unsigned i=32;i-->0;)mmr_sound_upgrade(&out->sound[i],(const void *)(bytes+4+i*110));
    }
    version=MMR_VERSION;memcpy(data,&version,2);return 1;
}
_Static_assert(sizeof(mmr_pattern_t)<=3840,"pattern payload too large");
_Static_assert(sizeof(mmr_kits_t)<=3840,"kit payload too large");
_Static_assert(sizeof(mmr_sounds_t)<=3840,"sound payload too large");
_Static_assert(P_COUNT<128 && NMODELS<=128,"byte sound format needs a new version");
static int mmr_sound_param(unsigned id)
{
    for(unsigned i=0;i<MMR_PATTERN_N;i++)if(id==MMR_PATTERN_PARAMS[i])return 0;
    for(unsigned i=0;i<MMR_KIT_N;i++)if(id==MMR_KIT_PARAMS[i])return 0;
    return id<P_COUNT;
}
static const param_desc_t *mmr_desc(unsigned model,unsigned id)
{
    if(id>=P_E0&&id<=P_E7)return &DMODELS[model].edit[id-P_E0];
    if(mmen_is_model(model)){const param_desc_t *d=mm_control_desc(id);if(d)return d;}
    return &TP[id];
}
static int mmr_name_valid(const char *name,unsigned n)
{
    for(unsigned i=0;i<n;i++){
        if(!name[i])return 1;
        if(!name_char_ok(name[i]))return 0;
    }
    return 1; /* fixed-width pattern names may occupy all bytes */
}
static void mmr_name_copy(char *dst,unsigned n,const char *src)
{
    memset(dst,0,n);
    if(src)for(unsigned i=0;i+1<n&&src[i];i++)dst[i]=name_char_ok(src[i])?src[i]:' ';
}
static int mmr_decode(uint8_t v,const param_desc_t *d)
{
    return d->min<0?(v>=128?(int)v-256:v):v;
}
static int mmr_sound_valid(const mmr_sound_t *s)
{
    unsigned model=s->p[P_MODEL];
    if(!perf_options_valid(&s->perf))return 0;
    if(s->used!=1||model>=NMODELS||s->edited>1||s->name[8]||!mmr_name_valid(s->name,8))return 0;
    for(unsigned id=0;id<P_COUNT;id++){
        if(!mmr_sound_param(id)){if(s->p[id])return 0;continue;}
        const param_desc_t *d=mmr_desc(model,id);
        int v=mmr_decode(s->p[id],d);
        if(v<d->min||v>d->max)return 0;
    }
    return 1;
}
static int mmr_sound_capture(mmr_sound_t *s,const track_t *t,const char *name)
{
    unsigned model=(unsigned)t->p[P_MODEL];
    if(model>=NMODELS||!perf_options_valid(&t->perf))return 0;
    for(unsigned id=0;id<P_COUNT;id++)if(mmr_sound_param(id)){
        const param_desc_t *d=mmr_desc(model,id);int v=t->p[id];
        if(v<d->min||v>d->max||v< -128||v>127)return 0;
    }
    memset(s,0,sizeof *s);s->used=1;
    mmr_name_copy(s->name,sizeof s->name,name);
    mmr_preset_set(s,preset_rec_enc(t->pre));s->edited=!!t->pre_ed;s->perf=t->perf;
    for(unsigned id=0;id<P_COUNT;id++)if(mmr_sound_param(id))s->p[id]=(uint8_t)t->p[id];
    return 1;
}
static int mmr_sound_apply(track_t *t,const mmr_sound_t *s)
{
    if(!mmr_sound_valid(s))return 0;
    unsigned model=s->p[P_MODEL],pre=preset_rec_dec(mmr_preset_ref(s));
    for(unsigned id=0;id<P_COUNT;id++)if(mmr_sound_param(id))t->p[id]=(int16_t)mmr_decode(s->p[id],mmr_desc(model,id));
    if(pre<PRE_NFACT&&PRE_FACTORY[pre].model!=model)pre=PRE_NONE;
    t->pre=(uint16_t)pre;t->pre_ed=s->edited;t->perf=s->perf;
    return 1;
}
static int mmr_kit_valid(const mmr_kit_t *kit)
{
    if(!multi_options_valid(&kit->multi))return 0;
    if(kit->used!=1||kit->name[NAME_LEN]||!mmr_name_valid(kit->name,NAME_LEN))return 0;
    for(unsigned k=0;k<NTRK;k++){
        if(!mmr_sound_valid(&kit->sound[k]))return 0;
        for(unsigned j=0;j<MMR_KIT_N;j++){
            const param_desc_t *d=&TP[MMR_KIT_PARAMS[j]];int v=kit->config[k][j];
            if(v<d->min||v>d->max)return 0;
        }
    }
    return 1;
}
static int mmr_kit_capture(mmr_kit_t *kit,const track_t *tracks,const char *name)
{
    memset(kit,0,sizeof *kit);mmr_name_copy(kit->name,sizeof kit->name,name);
    for(unsigned k=0;k<NTRK;k++){
        char label[9];preset_name_of(tracks[k].pre,label);
        if(!mmr_sound_capture(&kit->sound[k],&tracks[k],label))return 0;
        for(unsigned j=0;j<MMR_KIT_N;j++)kit->config[k][j]=tracks[k].p[MMR_KIT_PARAMS[j]];
    }
    kit->multi=song.multi;
    kit->used=1;if(mmr_kit_valid(kit))return 1;kit->used=0;return 0;
}
static int mmr_kit_apply(track_t *tracks,const mmr_kit_t *kit)
{
    if(!mmr_kit_valid(kit))return 0;
    song.multi=kit->multi;
    for(unsigned k=0;k<NTRK;k++){
        mmr_sound_apply(&tracks[k],&kit->sound[k]);
        for(unsigned j=0;j<MMR_KIT_N;j++)tracks[k].p[MMR_KIT_PARAMS[j]]=kit->config[k][j];
    }
    return 1;
}
static int seq_options_valid(const seq_options_t *s)
{
    return (s->arp_mode_order&7u)<=4&&(s->arp_mode_order>>3)<=4&&s->arp_ojmp<=7&&
        s->arp_len>=1&&s->arp_len<=16&&!(s->arp_flags&~31u)&&s->transpose>=-48&&s->transpose<=48&&
        (s->scale_key&15u)<12&&(s->scale_key>>4)<SEQ_NSCALES&&s->arp_speed>=1&&s->arp_speed<=96;
}
static int pattern_options_valid(const pattern_options_t *s)
{return (!s->swing||(s->swing>=50&&s->swing<=80))&&s->transpose>=-48&&s->transpose<=48&&s->scale<4&&s->key<12;}
static int mmr_pattern_valid(const mmr_pattern_t *p)
{
    if((p->version<1||p->version>MMR_PATTERN_VERSION)||p->used!=1||p->kit>=MMR_KITS||p->selected>=NTRK||p->reserved||
       !mmr_name_valid(p->name,sizeof p->name)||!motion_valid(&p->motion)||!chain_valid(&p->song))return 0;
    if(p->version>=3&&!pattern_options_valid(&p->options))return 0;
    if(p->version>=5&&!gen_pattern_valid(&p->gen_pat))return 0;
    for(unsigned k=0;k<NTRK;k++)if(p->version>=5&&!gen_options_valid(&p->gen[k]))return 0;
    for(unsigned k=0;k<NTRK;k++)if(p->version>=3&&!seq_options_valid(&p->seq[k]))return 0;
    for(unsigned i=0;i<G_COUNT;i++)if(p->g[i]<GP[i].min||p->g[i]>GP[i].max)return 0;
    for(unsigned k=0;k<NTRK;k++){
        for(unsigned j=0;j<MMR_PATTERN_N;j++){
            const param_desc_t *d=&TP[MMR_PATTERN_PARAMS[j]];int v=p->timing[k][j];
            if(v<d->min||v>d->max)return 0;
        }
        for(unsigned i=0;i<NSTEP;i++){
            const step_t *s=&p->step[k][i];
            if(s->cond>COND_MAX||s->nt<NT_MIN||s->nt>NT_MAX||
               (p->version>=2&&(s->rsv&~ST_MASK)))return 0;
        }
    }
    return 1;
}
static int mmr_pattern_capture(mmr_pattern_t *p,const track_t *tracks,const int16_t *globals,
                              const motion_store_t *motion,const chain_config_t *song_config,
                              unsigned selected,unsigned kit,const char *name)
{
    memset(p,0,sizeof *p);p->version=MMR_PATTERN_VERSION;p->used=1;
    if(selected>=NTRK||kit>=MMR_KITS){p->used=0;return 0;}
    p->selected=(uint8_t)selected;p->kit=(uint8_t)kit;
    /* Pattern names have the existing twelve-byte project representation. */
    if(name)for(unsigned i=0;i<NAME_LEN&&name[i];i++)p->name[i]=name_char_ok(name[i])?name[i]:' ';
    memcpy(p->g,globals,sizeof p->g);p->motion=*motion;p->song=*song_config;
    p->options=song.seq;
    p->gen_pat=song.gen;
    for(unsigned k=0;k<NTRK;k++){
        for(unsigned j=0;j<MMR_PATTERN_N;j++)p->timing[k][j]=tracks[k].p[MMR_PATTERN_PARAMS[j]];
        memcpy(p->step[k],tracks[k].step,sizeof p->step[k]);
        p->seq[k]=tracks[k].seq;
        p->gen[k]=tracks[k].gen;
    }
    if(mmr_pattern_valid(p))return 1;p->used=0;return 0;
}
static int mmr_pattern_apply(track_t *tracks,int16_t *globals,motion_store_t *motion,
                            chain_config_t *song_config,const mmr_pattern_t *p)
{
    if(!mmr_pattern_valid(p))return 0;
    memcpy(globals,p->g,sizeof p->g);*motion=p->motion;*song_config=p->song;
    song.seq=p->version>=3?p->options:(pattern_options_t){0};
    if(p->version>=5)song.gen=p->gen_pat;else gen_pattern_default(&song.gen);
    for(unsigned k=0;k<NTRK;k++){
        for(unsigned j=0;j<MMR_PATTERN_N;j++)tracks[k].p[MMR_PATTERN_PARAMS[j]]=p->timing[k][j];
        memcpy(tracks[k].step,p->step[k],sizeof p->step[k]);
        if(p->version>=3)tracks[k].seq=p->seq[k];else seq_options_default(&tracks[k].seq);
        if(p->version>=5)tracks[k].gen=p->gen[k];else gen_options_default(&tracks[k].gen);
        if(p->version==1)for(unsigned i=0;i<NSTEP;i++)tracks[k].step[i].rsv=0;
    }
    return 1; /* selected track and kit link are returned in the validated record */
}
static int mmr_sounds_valid(const mmr_sounds_t *b)
{
    if(b->version!=MMR_VERSION||b->reserved)return 0;
    for(unsigned i=0;i<MMR_SOUNDS_PER_OBJECT;i++)if(b->sound[i].used&&!mmr_sound_valid(&b->sound[i]))return 0;
    return 1;
}
static int mmr_kits_valid(const mmr_kits_t *b)
{
    if(b->version!=MMR_VERSION||b->reserved)return 0;
    for(unsigned i=0;i<MMR_KITS_PER_OBJECT;i++)if(b->kit[i].used&&!mmr_kit_valid(&b->kit[i]))return 0;
    return 1;
}
#endif
