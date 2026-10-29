/* SPDX-License-Identifier: GPL-3.0-only
 * MNM3/4/5/6/7/8 migration and MNM9 (GEN) through the real project/storage code. */
#define FELUCCA_FLASH 1
#include <stdint.h>
static uint8_t hflash[0x100000];
#define SMP_USER_XIP(k) ((const uint8_t *)hflash + 0xA0000u + (k) * 0x14000u)
#include "ui_host.h"
#include <assert.h>

static mnm_project_v3_t legacy;
static project_t expected;
static mnm_project_v5_t recent;
static mnm_project_v6_t compact;
static union { project_t q; uint8_t bytes[3840]; } decode_buf;

static void fixture(void)
{
    uint32_t k, i;
    bench_reset();
    flash_ok = 0;
    transport_req = 0;
    song.playing = 0;
    memset(&chain, 0, sizeof chain);
    memset(&mo, 0, sizeof mo);
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_COUNT; i++) trk[k].p[i] = TP[i].def;
        drum_set_model(&trk[k], DM_MMSAW);
        seq_options_default(&trk[k].seq);
        perf_options_default(&trk[k].perf);
        trk[k].p[P_PAN] = -23;
        trk[k].p[P_E7] = -1;
        trk[k].p[P_NOTE] = (int16_t)(k * 2);
        trk[k].pre = k == 1 ? PRE_NFACT + 7u : PRE_NONE;
        trk[k].pre_ed = k & 1u;
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &trk[k].step[i];
            memset(s, 0, sizeof *s);
            s->on = i & 1u; s->acc = (i >> 1) & 1u;
            s->nt = (int8_t)((int)(i % 25u) - 12);
            s->rat = i % 4u; s->cond = i % COND_MAX;
            s->slide = (i >> 2) & 1u; s->tie = (i >> 3) & 1u;
        }
    }
    mo.s.count = 2; mo.s.on = 3;
    mo.s.ev[0] = (motion_event_t){0, 1, P_E0, 42};
    mo.s.ev[1] = (motion_event_t){1, 5, P_PAN, 7};
    assert(motion_valid(&mo.s));
    chain.cfg.count = 2; chain.cfg.loop = 1;
    chain.cfg.row[0] = (chain_row_t){1, 3};
    chain.cfg.row[1] = (chain_row_t){3, 2};
    name_set(chain.name, "OLD SONG");
    song.sel = 4;
    memset(&song.seq,0,sizeof song.seq);
    multi_options_default(&song.multi);
    project_save(0);
    expected = proj_slot[0];
    memset(&legacy, 0, sizeof legacy);
    legacy.magic = PROJ_V3_MAGIC; legacy.size = sizeof legacy;
    memcpy(legacy.g, expected.g, sizeof legacy.g);
    legacy.sel = expected.sel;
    for (k = 0; k < NTRK; k++) {
        for(i=0;i<PROJ_V3_PARAMS;i++)legacy.t[k].p[i]=proj_value(&expected.t[k],i);
        memcpy(legacy.t[k].step, expected.t[k].step, sizeof legacy.t[k].step);
    }
    legacy.motion = expected.motion; memcpy(&legacy.song,&expected.song,sizeof legacy.song);
    memcpy(legacy.name, expected.name, sizeof legacy.name);
    memcpy(legacy.pre, expected.pre, sizeof legacy.pre);
    memcpy(legacy.pre_ed, expected.pre_ed, sizeof legacy.pre_ed);
    legacy.sum = proj_hash(&legacy, sizeof legacy - 4u);
}
static void test_decode(void)
{
    project_t *q=&decode_buf.q;
    assert(sizeof legacy==3624u && sizeof recent==3836u && sizeof *q<=3840u);
    memcpy(q,&legacy,sizeof legacy);
    assert(proj_decode(q,sizeof legacy));
    assert(!memcmp(q,&expected,sizeof *q));
    assert(proj_decode(q,sizeof *q));
    q->t[3].p[P_PAN]^=1;assert(!proj_decode(q,sizeof *q));
    for(unsigned i=0;i<sizeof legacy;i++) {
        memcpy(q,&legacy,sizeof legacy);decode_buf.bytes[i]^=1;
        assert(!proj_decode(q,sizeof legacy));
    }
    memcpy(q,&legacy,sizeof legacy);
    assert(!proj_decode(q,sizeof legacy-1));assert(!proj_decode(q,sizeof legacy+1));
    memset(&recent,0,sizeof recent);recent.magic=PROJ_V5_MAGIC;recent.size=sizeof recent;
    memcpy(recent.g,expected.g,sizeof recent.g);recent.sel=expected.sel;
    for(unsigned k=0;k<NTRK;k++) {
        for(unsigned i=0;i<97;i++)recent.t[k].p[i]=proj_value(&expected.t[k],i);
        memcpy(recent.t[k].step,expected.t[k].step,sizeof recent.t[k].step);
        recent.t[k].step[0].rsv=ST_NOAMP|ST_NOLFO;
    }
    recent.motion=expected.motion;memcpy(&recent.song,&expected.song,sizeof recent.song);
    memcpy(recent.name,expected.name,sizeof recent.name);
    memcpy(recent.pre,expected.pre,sizeof recent.pre);memcpy(recent.pre_ed,expected.pre_ed,sizeof recent.pre_ed);
    for(unsigned version=4;version<=5;version++) {
        recent.magic=version==4?PROJ_V4_MAGIC:PROJ_V5_MAGIC;
        recent.sum=proj_hash(&recent,sizeof recent-4);
        memcpy(q,&recent,sizeof recent);assert(proj_decode(q,sizeof recent));
        for(unsigned k=0;k<NTRK;k++) {
            assert(proj_value(&q->t[k],P_PAN)==-23);
            assert(proj_value(&q->t[k],P_E7)==-1);
            assert(q->t[k].step[0].rsv==(version==5?(ST_NOAMP|ST_NOLFO):0));
            assert(!memcmp(&q->seq[k],&expected.seq[k],sizeof q->seq[k]));
        }
        for(unsigned i=0;i<sizeof recent;i++) {
            memcpy(q,&recent,sizeof recent);decode_buf.bytes[i]^=1;
            assert(!proj_decode(q,sizeof recent));
        }
        memcpy(q,&recent,sizeof recent);assert(!proj_decode(q,sizeof recent-1));
        memcpy(q,&recent,sizeof recent);assert(!proj_decode(q,sizeof recent+1));
    }
}

static void test_flash(void)
{
    uint32_t k;
    project_t saved;
    memset(hflash, 0xFF, sizeof hflash);
    memset(proj_slot, 0xCC, sizeof proj_slot);
    flash_ok = 1;
    for (k = 0; k < 4; k++) assert(!st_save(OBJ_PROJECT0 + k, &legacy, sizeof legacy));
    for (k = 0; k < 4; k++) {
        proj_fetch(k);
        assert(!memcmp(&proj_slot[k], &expected, sizeof expected));
    }
    memset(trk[0].p, 0, sizeof trk[0].p);
    assert(proj_load(2, 0));
    for (k = 0; k < NTRK; k++) {
        for(unsigned i=0;i<P_COUNT;i++)assert(trk[k].p[i]==proj_value(&expected.t[k],i));
        assert(!memcmp(trk[k].step, expected.t[k].step, sizeof trk[k].step));
    }
    assert(!memcmp(&mo.s, &legacy.motion, sizeof mo.s));
    assert(!memcmp(&chain.cfg, &legacy.song, sizeof legacy.song));
    assert(!memcmp(chain.cfg.options,expected.song.options,sizeof chain.cfg.options));
    assert(!strcmp(chain.name, "OLD SONG") && song.sel == 4);
    assert(trk[1].pre == PRE_NFACT + 7u && trk[1].pre_ed == 1);
    trk[2].seq.arp_offset[15]=-17;trk[2].seq.arp_ojmp=5;
    trk[2].seq.swing[7]=3;song.seq.swing=67;song.seq.transpose=-12;
    trk[2].perf.dest[0]=P_FCUT+1;trk[2].perf.amount[0]=-43;
    trk[2].perf.route_flags=PERF_HP_OFF|4;
    song.multi.mode=MULTI_SPLIT;song.multi.split_key=72;song.multi.attack=33;
    chain.cfg.options[0]=(chain_row_options_t){-12,5,17,3,135,2,1};
    project_save(2);
    assert(st_load(OBJ_PROJECT0 + 2u, &saved, sizeof saved) == sizeof saved);
    assert(proj_ok(&saved));
    assert(saved.song.options[0].transpose==-12 && saved.song.options[0].bpm==135);
    assert(saved.seq[2].arp_offset[15]==-17 && saved.seq[2].arp_ojmp==5);
    assert(saved.options.swing==67 && saved.options.transpose==-12);
    assert(saved.perf[2].dest[0]==P_FCUT+1&&saved.perf[2].amount[0]==-43);
    assert(saved.multi.mode==MULTI_SPLIT&&saved.multi.split_key==72);
    memset(&proj_slot[2], 0, sizeof proj_slot[2]);
    proj_fetch(2);
    assert(!memcmp(&saved, &proj_slot[2], sizeof saved));
    assert(proj_load(2,0));assert(trk[2].seq.arp_offset[15]==-17&&song.seq.swing==67);
    assert(trk[2].perf.amount[0]==-43&&trk[2].perf.route_flags==(PERF_HP_OFF|4));
    assert(song.multi.split_key==72&&song.multi.attack==33);
    assert(chain.cfg.options[0].length==17 && chain.cfg.options[0].jump==1);
    /* Re-fetch an oversized legacy record between compact neighbors, last slot too. */
    for(k=0;k<4;k++) {
        assert(!st_save(OBJ_PROJECT0+k,&recent,sizeof recent));
        project_t neighbors[4];memcpy(neighbors,proj_slot,sizeof neighbors);
        proj_fetch(k);assert(proj_ok(&proj_slot[k]));
        for(unsigned j=0;j<4;j++)if(j!=k)assert(!memcmp(&neighbors[j],&proj_slot[j],sizeof saved));
    }
    legacy.sum ^= 1;
    assert(!st_save(OBJ_PROJECT0, &legacy, sizeof legacy));
    proj_fetch(0);
    assert(!proj_ok(&proj_slot[0]));
    legacy.sum ^= 1;
    assert(!st_save(OBJ_PROJECT0, &legacy, sizeof legacy - 1u));
    proj_fetch(0);
    assert(!proj_ok(&proj_slot[0]));
}
static void test_values_and_validation(void)
{
    proj_trk_t t;memset(&t,0,sizeof t);
    for(unsigned m=0;m<NMODELS;m++) {
        t.p[P_MODEL]=(uint8_t)m;
        for(unsigned id=0;id<P_COUNT;id++) {
            const param_desc_t *d=proj_desc(m,id);
            assert(d->min>=-128 && d->max<=127);
            if(id==P_MODEL)continue;
            t.p[id]=proj_byte(m,id,d->min);assert(proj_value(&t,id)==d->min);
            t.p[id]=proj_byte(m,id,d->max);assert(proj_value(&t,id)==d->max);
        }
    }
    project_t *q=&decode_buf.q;*q=expected;
    q->seq[0].arp_len=0;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));
    *q=expected;q->options.swing=49;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));
    /* Source models use signed common controls even where legacy descriptors differ. */
    recent.t[1].p[P_MODEL]=DM_M2_GSIN;recent.t[1].p[P_PAN]=-64;
    recent.t[1].p[P_MVOL]=127;recent.t[1].p[P_E7]=64;
    recent.sum=proj_hash(&recent,sizeof recent-4);memcpy(q,&recent,sizeof recent);
    assert(proj_decode(q,sizeof recent));
    assert(proj_value(&q->t[1],P_PAN)==-64 && proj_value(&q->t[1],P_MVOL)==127);
    assert(proj_value(&q->t[1],P_E7)==64);
    printf("MNM9 %zu bytes; project/chain union %zu bytes\n",sizeof(project_t),sizeof proj_memory);
}
static void test_mnm6(void)
{
    assert(sizeof compact==3456);
    memset(&compact,0,sizeof compact);
    memcpy(&compact,&expected,__builtin_offsetof(mnm_project_v6_t,name));
    memcpy(compact.name,expected.name,sizeof compact.name);
    memcpy(compact.pre,expected.pre,sizeof compact.pre);
    memcpy(compact.pre_ed,expected.pre_ed,sizeof compact.pre_ed);
    compact.magic=PROJ_V6_MAGIC;compact.size=sizeof compact;
    compact.t[2].step[9].rsv=ST_NOPITCH|ST_NOAMP;
    compact.seq[2][0]=0x71; /* swing mask: verify old options survive unchanged */
    compact.sum=proj_hash(&compact,sizeof compact-4);
    project_t *q=&decode_buf.q;
    memset(&decode_buf,0xA5,sizeof decode_buf);memcpy(q,&compact,sizeof compact);
    assert(proj_decode(q,sizeof compact)&&proj_ok(q));
    assert(q->seq[2].swing[0]==0x71&&q->t[2].step[9].rsv==(ST_NOPITCH|ST_NOAMP));
    perf_options_t empty={0};multi_options_t def;multi_options_default(&def);
    for(unsigned k=0;k<NTRK;k++)assert(!memcmp(&q->perf[k],&empty,sizeof empty));
    assert(!memcmp(&q->multi,&def,sizeof def));
    for(unsigned i=0;i<sizeof compact;i++) {
        memcpy(q,&compact,sizeof compact);decode_buf.bytes[i]^=1;
        assert(!proj_decode(q,sizeof compact));
    }
    memcpy(q,&compact,sizeof compact);assert(!proj_decode(q,sizeof compact-1));
    assert(!proj_decode(q,sizeof compact+1));
    flash_ok=1;assert(!st_save(OBJ_PROJECT0+3,&compact,sizeof compact));
    proj_fetch(3);assert(proj_ok(&proj_slot[3])&&proj_slot[3].seq[2].swing[0]==0x71);
    *q=expected;q->perf[0].route_flags=7;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));
    *q=expected;q->perf[0].amount[1]=-65;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));
    *q=expected;q->perf[0].dest[0]=P_MODEL+1;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));
    *q=expected;q->multi.first_upper=6;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));
    *q=expected;q->multi.mode=5;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));
    *q=expected;q->multi.attack=128;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));
    *q=expected;q->multi.mode=MULTI_SPLIT;q->sum^=1;assert(!proj_decode(q,sizeof *q));
}
static void test_mnm7(void)
{
    mnm_project_v7_t old;
    assert(sizeof old==3520 && sizeof(project_t)==3680 && PROJ_V8_SIZE==3648);
    memset(&old,0,sizeof old);
    memcpy(&old,&expected,__builtin_offsetof(mnm_project_v7_t,name));
    memcpy(old.name,expected.name,sizeof old.name);
    memcpy(old.pre,expected.pre,sizeof old.pre);
    memcpy(old.pre_ed,expected.pre_ed,sizeof old.pre_ed);
    memcpy(old.perf,expected.perf,sizeof old.perf);
    memcpy(old.multi,&expected.multi,sizeof old.multi);
    old.perf[3][0]=P_FCUT+1; old.perf[3][4]=(uint8_t)-43;
    old.multi[0]=MULTI_SPLIT;old.multi[1]=74;
    old.seq[4][0]=0x37;old.t[3].step[2].rsv=ST_NOAMP|ST_NOPITCH;
    old.song.rsv[0]=0xA5;old.song.rsv[1]=0xCC; /* historically reserved */
    old.magic=PROJ_V7_MAGIC;old.size=sizeof old;
    old.sum=proj_hash(&old,sizeof old-4);
    project_t *q=&decode_buf.q;
    memcpy(q,&old,sizeof old);assert(proj_decode(q,sizeof old)&&proj_ok(q));
    assert(!memcmp(q->name,expected.name,sizeof q->name));
    assert(!memcmp(q->pre,expected.pre,sizeof q->pre));
    assert(!memcmp(q->pre_ed,expected.pre_ed,sizeof q->pre_ed));
    assert(q->perf[3].amount[0]==-43 && q->perf[3].dest[0]==P_FCUT+1);
    assert(q->multi.mode==MULTI_SPLIT&&q->multi.split_key==74);
    assert(q->seq[4].swing[0]==0x37&&q->t[3].step[2].rsv==(ST_NOAMP|ST_NOPITCH));
    assert(!q->song.rsv[0]&&!q->song.rsv[1]);
    for(unsigned i=0;i<sizeof q->song.options;i++)assert(!((uint8_t*)q->song.options)[i]);
    gen_options_t goff;gen_options_default(&goff);gen_pattern_t gpoff;gen_pattern_default(&gpoff);
    for(unsigned k=0;k<NTRK;k++)assert(!memcmp(&q->gen[k],&goff,sizeof goff));   /* MNM9: GEN off */
    assert(!memcmp(&q->gen_pat,&gpoff,sizeof gpoff));
    for(unsigned i=0;i<sizeof old;i++){
        memcpy(q,&old,sizeof old);decode_buf.bytes[i]^=1;
        assert(!proj_decode(q,sizeof old));
    }
    memcpy(q,&old,sizeof old);assert(!proj_decode(q,sizeof old-1));
    memcpy(q,&old,sizeof old);assert(!proj_decode(q,sizeof old+1));
    flash_ok=1;assert(!st_save(OBJ_PROJECT0+1,&old,sizeof old));
    proj_fetch(1);assert(proj_ok(&proj_slot[1])&&proj_slot[1].multi.split_key==74);
    old.song.row[0].repeat=0;old.sum=proj_hash(&old,sizeof old-4);
    memcpy(q,&old,sizeof old);assert(proj_decode(q,sizeof old)&&proj_ok(q)&&!q->song.count);
    assert(q->multi.split_key==74&&q->perf[3].amount[0]==-43);
    *q=expected;q->song.options[0].bpm=39;q->sum=proj_sum(q);
    assert(proj_decode(q,sizeof *q)&&proj_ok(q)&&!q->song.count&&!memcmp(q->t,expected.t,sizeof q->t));
    *q=expected;q->song.options[0].jump=3;q->sum=proj_sum(q);
    assert(proj_decode(q,sizeof *q)&&proj_ok(q)&&!q->song.count&&!memcmp(q->t,expected.t,sizeof q->t));
}
/* MNM8: MNM9 without GEN, its sum where GEN starts; loads with GEN off, everything else as it was */
static void test_mnm8(void)
{
    static union { project_t q; uint8_t bytes[3840]; } old;
    memset(&old,0,sizeof old);
    memcpy(&old.q,&expected,PROJ_V8_SIZE-4u);
    old.q.song.options[1].bpm=133;old.q.song.options[1].jump=0;old.q.multi.split_key=71;
    old.q.magic=PROJ_V8_MAGIC;old.q.size=PROJ_V8_SIZE;
    uint32_t sum=proj_hash(&old.q,PROJ_V8_SIZE-4u);memcpy(old.bytes+PROJ_V8_SIZE-4u,&sum,4);
    project_t *q=&decode_buf.q;
    memcpy(q,&old,PROJ_V8_SIZE);assert(proj_decode(q,PROJ_V8_SIZE)&&proj_ok(q)&&q->magic==PROJ_MAGIC);
    assert(!memcmp(q->t,expected.t,sizeof q->t)&&!memcmp(q->seq,expected.seq,sizeof q->seq));
    assert(!memcmp(q->name,expected.name,sizeof q->name)&&!memcmp(&q->motion,&expected.motion,sizeof q->motion));
    assert(q->song.options[1].bpm==133&&q->multi.split_key==71);
    gen_options_t goff;gen_options_default(&goff);gen_pattern_t gpoff;gen_pattern_default(&gpoff);
    for(unsigned k=0;k<NTRK;k++)assert(!memcmp(&q->gen[k],&goff,sizeof goff));
    assert(!memcmp(&q->gen_pat,&gpoff,sizeof gpoff));
    for(unsigned i=0;i<PROJ_V8_SIZE;i++){                /* any damaged byte: refused */
        memcpy(q,&old,PROJ_V8_SIZE);decode_buf.bytes[i]^=1;
        assert(!proj_decode(q,PROJ_V8_SIZE));
    }
    memcpy(q,&old,PROJ_V8_SIZE);assert(!proj_decode(q,PROJ_V8_SIZE-1));
    memcpy(q,&old,PROJ_V8_SIZE);assert(!proj_decode(q,PROJ_V8_SIZE+1));
    flash_ok=1;assert(!st_save(OBJ_PROJECT0+2,&old,PROJ_V8_SIZE));   /* through flash, as the FM-1 reads it */
    proj_fetch(2);assert(proj_ok(&proj_slot[2])&&proj_slot[2].multi.split_key==71&&gen_seed(&proj_slot[2].gen_pat)==GEN_SEED_DEFAULT);
    *q=expected;q->gen[1].marbles=101;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));   /* MNM9's own GEN checked */
    *q=expected;q->gen_pat.journey=5;q->sum=proj_sum(q);assert(!proj_decode(q,sizeof *q));
    *q=expected;q->gen[2]=(gen_options_t){4,100,-3,1};gen_seed_set(&q->gen_pat,99);q->sum=proj_sum(q);
    assert(proj_decode(q,sizeof *q)&&q->gen[2].kind==4&&gen_seed(&q->gen_pat)==99);
}
int main(void)
{
    fixture(); test_decode(); test_flash();test_values_and_validation();test_mnm6();test_mnm7();test_mnm8();
    puts("project_migration_test: all passed");
    return 0;
}
