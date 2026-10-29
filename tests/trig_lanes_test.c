/* SPDX-License-Identifier: GPL-3.0-only
 * Selective sequencer trigger lanes through the production voice allocator. */
#include "ui_host.h"
#include <assert.h>
static void fresh(unsigned model)
{
    ui_host_init();transport_req=0;song.playing=0;
    for(unsigned k=0;k<NTRK;k++){
        drum_set_model(&trk[k],DM_MMGND);memset(trk[k].step,0,sizeof trk[k].step);
    }
    drum_set_model(&trk[0],model);
    if(mmen_is_model(model))mmen_default(&trk[0]);
    trk[0].p[P_MUTE]=0;trk[0].p[P_SRC]=0;
}
static unsigned active(void)
{
    unsigned n=0;for(unsigned i=0;i<NDV;i++)n+=!!trk[0].v[i].active;return n;
}
static void test_legacy_all(void)
{
    const unsigned models[]={DM_K808,DM_MMSAW,DM_ACID,DM_M2_GSIN};
    static track_t expected;
    static mma_track_t source;
    for(unsigned i=0;i<sizeof models/sizeof *models;i++){
        fresh(models[i]);drum_play(&trk[0],103,7);expected=trk[0];source=mmen_state[0];
        fresh(models[i]);drum_play_masked(&trk[0],103,7,0);
        assert(!memcmp(&expected,&trk[0],sizeof expected));
        assert(!memcmp(&source,&mmen_state[0],sizeof source));
    }
}
static void test_legacy_lanes(void)
{
    fresh(DM_MMSAW);track_t *t=&trk[0];
    t->p[P_LFO1+LF_TRIG]=LT_TRIG;
    t->lfo[0].ph=123456;t->lfo[0].fresh=0;t->nt=t->trig_nt=3;t->fenv=123;
    drum_play_masked(t,96,9,ST_NOAMP|ST_NOFLT|ST_NOPITCH);
    assert(!active()&&t->nt==3&&t->fenv==123&&t->lfo[0].ph==0);
    t->lfo[0].ph=654321;t->lfo[0].fresh=0;
    drum_play_masked(t,96,8,ST_NOAMP|ST_NOLFO|ST_NOPITCH);
    assert(!active()&&t->nt==3&&t->fenv!=123&&t->lfo[0].ph==654321&&!t->lfo[0].fresh);
    t->fenv=222;
    drum_play_masked(t,96,11,ST_NOAMP|ST_NOFLT|ST_NOLFO);
    assert(!active()&&t->nt==11&&t->fenv==222&&t->lfo[0].ph==654321);
    drum_play_masked(t,96,5,ST_NOFLT|ST_NOLFO|ST_NOPITCH);
    assert(active()&&t->nt==11&&t->fenv==222&&t->lfo[0].ph==654321);
    uint32_t age=t->v[0].age;
    drum_play_masked(t,96,12,ST_NOAMP|ST_NOFLT|ST_NOLFO);
    assert(t->v[0].age==age&&t->nt==12);
}
static void test_source_lanes(void)
{
    fresh(DM_M2_GSIN);track_t *t=&trk[0];
    t->p[P_AHOLD]=127;t->p[P_AREL]=0;
    drum_play(t,100,0);render_mix(0,0,CTL);
    mma_track_t *s=&mmen_state[0];
    uint32_t age=t->v[0].age,aenv=s->aenv;unsigned astage=s->astage,gate=s->gate;
    s->fenv=765432;s->fstage=2;
    drum_play_masked(t,70,7,ST_NOAMP|ST_NOLFO|ST_NOPITCH);
    assert(t->v[0].age==age&&s->aenv==aenv&&s->astage==astage&&s->gate==gate);
    assert(s->fenv==0&&s->fstage==1&&t->nt==0&&s->note==60);
    s->fenv=876543;s->fstage=2;
    drum_play_masked(t,80,9,ST_NOFLT|ST_NOLFO|ST_NOPITCH);
    assert(s->fenv==876543&&s->fstage==2&&s->note==60&&s->gate);
    drum_play_masked(t,80,12,ST_NOAMP|ST_NOFLT|ST_NOLFO);
    assert(t->nt==12&&s->note==72&&s->fenv==876543);
    age=t->v[0].age;
    drum_play_masked(t,127,-12,ST_NOTEOFF);
    assert(!s->gate&&t->v[0].age==age&&t->nt==12);
    render_mix(0,0,44100);assert(!active());
    fresh(DM_M2_GSIN);
    drum_play_masked(&trk[0],96,4,ST_NOAMP|ST_NOFLT|ST_NOLFO);
    assert(!active());
}
static void test_pitchless_after_arp(void)
{
    const unsigned models[]={DM_MMSAW,DM_M2_GSIN};
    for(unsigned i=0;i<sizeof models/sizeof *models;i++){
        fresh(models[i]);track_t *t=&trk[0];
        drum_play(t,96,12);
        assert(t->trig_nt==12);
        t->nt=0; /* ARP restores its parent note after starting a child. */
        drum_play_masked(t,96,-7,ST_NOPITCH);
        assert(t->nt==12&&t->trig_nt==12&&active());
        if(mmen_is_model(models[i]))assert(mmen_state[0].note==72);
        t->nt=0;
        drum_play_masked(t,96,-7,ST_NOAMP|ST_NOPITCH);
        assert(t->trig_nt==12);
        if(mmen_is_model(models[i]))assert(mmen_state[0].note==72);
    }
}
static void test_idle_filter(void)
{
    fresh(DM_M2_GSIN);track_t *t=&trk[0];t->p[P_FATK]=64;
    drum_play_masked(t,96,0,ST_NOAMP|ST_NOLFO|ST_NOPITCH);
    mma_track_t *s=&mmen_state[0];
    assert(!active()&&!s->sounding&&!s->gate&&s->fstage==1);
    uint32_t start=s->fenv;
    render_mix(0,0,CTL);
    assert(s->fenv>start&&!active()&&!s->sounding&&!s->gate&&!s->aenv);
    start=s->fenv;render_mix(0,0,CTL);
    assert(s->fenv>start&&!active());
    for(unsigned n=0;n<CTL;n++)assert(!mmen_left[0][n]&&!mmen_right[0][n]);
    /* Note-off suppresses every other lane, even with a pending roll. */
    t->rat_n=4;t->rat_k=1;t->gate_n=100;t->nt=5;
    drum_play_masked(t,127,12,ST_NOTEOFF|ST_NOAMP|ST_NOFLT|ST_NOLFO|ST_NOPITCH);
    assert(!t->rat_n&&!t->gate_n&&!active()&&t->nt==5);
}
static void test_step_and_ratchet(void)
{
    fresh(DM_MMSAW);track_t *t=&trk[0];step_t *s=&t->step[0];
    t->nt=t->trig_nt=4;t->seq_idx=0;t->seq_cnt=0;s->on=1;s->nt=12;s->rat=2;
    s->rsv=ST_NOPITCH|ST_NOFLT|ST_NOLFO;t->fenv=321;
    step_fire(t,300);assert(active()&&t->nt==4&&t->fenv==321&&t->rat_n==3);
    /* Editing the step after its first hit must not alter an in-flight roll. */
    s->rsv=0;s->nt=-12;t->fenv=654;t->seq_pos=100;
    rat_due(t,300);assert(t->rat_k==2&&t->nt==4&&t->fenv==654);
    t->seq_pos=200;rat_due(t,300);assert(t->rat_k==3&&t->fenv==654);
    fresh(DM_M2_GSIN);t=&trk[0];drum_play(t,96,0);
    s=&t->step[0];s->on=1;s->rat=3;s->rsv=ST_NOTEOFF;t->seq_idx=0;
    step_fire(t,400);assert(!mmen_state[0].gate&&!t->rat_n);
}
static void test_lane_ui(void)
{
    fresh(DM_MMSAW);ui.home=ui.menu=0;
    for(unsigned p=0;p<NPAGES;p++)if(PAGES[p].scope==SC_GRID){ui.page=p;break;}
    assert(grid_mode()&&ui.lane==0);
    assert(!strcmp(STEP_LANES[0],"ALL")&&!strcmp(STEP_LANES[1],"AMP")&&
           !strcmp(STEP_LANES[2],"FILTER")&&!strcmp(STEP_LANES[3],"LFO"));
    for(unsigned lane=1;lane<=3;lane++){
        edit_param(1,1);assert(ui.lane==lane);
        step_grab(lane);step_tap(lane);
        step_t *st=&trk[0].step[lane];
        assert(st->on&&st->rsv==((7u&~(1u<<(lane-1)))|ST_NOPITCH)&&step_lane_on(*st));
        step_grab(lane);step_tap(lane);assert(!st->on);
    }
    edit_param(1,1);assert(ui.lane==3);
    for(unsigned i=0;i<4;i++)edit_param(1,-1);
    assert(!ui.lane);step_grab(0);step_tap(0);assert(trk[0].step[0].on&&!trk[0].step[0].rsv);
    /* Combining lanes preserves the existing ones and independent step data. */
    trk[0].step[0].nt=9;trk[0].step[0].rat=2;
    edit_param(1,1);step_grab(0);step_tap(0);
    assert(trk[0].step[0].on&&trk[0].step[0].rsv==ST_NOAMP&&trk[0].step[0].nt==9&&trk[0].step[0].rat==2);
    ui.lane=0;memset(&trk[0].step[4],0,sizeof(step_t));
    keys(1u<<STEP_KEY[4]);ui_frame();host_ticks+=600000u*FM1_TICKS_PER_US;ui_frame();
    assert(held_step()==4);
    const char *names[]={"ALL","AMP","FILTER","LFO","TRIGLESS","NOTE OFF","PITCHLESS","LOCKS"};
    const unsigned flags[]={0,14,13,11,7,16,8,15};
    for(unsigned type=1;type<8;type++){
        turn(EN_SELECT,1);ui_frame();
        assert(trk[0].step[4].on&&trk[0].step[4].rsv==flags[type]);
        assert(STEP_TYPES[type]==flags[type]&&!strcmp(STEP_TYPE_NAMES[type],names[type]));
    }
    keys(0);ui_frame();assert(trk[0].step[4].on&&trk[0].step[4].rsv==15);
}
static void test_flag_storage(void)
{
    static union {mnm_project_v5_t wire;project_t current;} old;
    static mmr_pattern_t pattern;
    fresh(DM_MMSAW);memset(&chain.cfg,0,sizeof chain.cfg);
    for(unsigned k=0;k<NTRK;k++)for(unsigned i=0;i<NSTEP;i++){
        trk[k].step[i].on=1;trk[k].step[i].rsv=(uint8_t)(i&ST_MASK);
    }
    project_save(0);assert(proj_slot[0].magic==PROJ_MAGIC&&PROJ_MAGIC==0x394D4E4Du);
    memset(trk[0].step,0,sizeof trk[0].step);project_load(0);transport_req=0;
    for(unsigned k=0;k<NTRK;k++)for(unsigned i=0;i<NSTEP;i++)assert(trk[k].step[i].rsv==(i&ST_MASK));
    memset(&old,0,sizeof old);old.wire.magic=PROJ_V4_MAGIC;old.wire.size=sizeof old.wire;
    memcpy(old.wire.g,proj_slot[0].g,sizeof old.wire.g);
    for(unsigned k=0;k<NTRK;k++)for(unsigned i=0;i<P_COUNT;i++)old.wire.t[k].p[i]=proj_value(&proj_slot[0].t[k],i);
    for(unsigned k=0;k<NTRK;k++)for(unsigned i=0;i<NSTEP;i++)old.wire.t[k].step[i].rsv=255;
    old.wire.sum=proj_hash(&old.wire,sizeof old.wire-4);assert(proj_decode(&old.current,sizeof old.wire)&&proj_ok(&old.current));
    for(unsigned k=0;k<NTRK;k++)for(unsigned i=0;i<NSTEP;i++)assert(!old.current.t[k].step[i].rsv);
    assert(mmr_pattern_capture(&pattern,trk,song.g,&mo.s,&chain.cfg,0,0,"FLAGS"));
    assert(pattern.version==5&&MMR_PATTERN_VERSION==5);
    memset(trk[0].step,0,sizeof trk[0].step);
    assert(mmr_pattern_apply(trk,song.g,&mo.s,&chain.cfg,&pattern));
    for(unsigned i=0;i<NSTEP;i++)assert(trk[0].step[i].rsv==(i&ST_MASK));
    pattern.step[0][0].rsv=32;assert(!mmr_pattern_valid(&pattern));
    step_t keep=trk[0].step[0];assert(!mmr_pattern_apply(trk,song.g,&mo.s,&chain.cfg,&pattern));
    assert(!memcmp(&keep,&trk[0].step[0],sizeof keep));
    pattern.version=1;pattern.step[0][0].rsv=255;assert(mmr_pattern_valid(&pattern));
    assert(mmr_pattern_apply(trk,song.g,&mo.s,&chain.cfg,&pattern));
    for(unsigned k=0;k<NTRK;k++)for(unsigned i=0;i<NSTEP;i++)assert(!trk[k].step[i].rsv);
    trk[0].step[0].rsv=128;
    assert(!mmr_pattern_capture(&pattern,trk,song.g,&mo.s,&chain.cfg,0,0,"BAD FLAGS"));
}
int main(void)
{
    assert(ST_NOAMP==1&&ST_NOFLT==2&&ST_NOLFO==4&&ST_NOPITCH==8&&ST_NOTEOFF==16);
    test_legacy_all();test_legacy_lanes();test_source_lanes();test_pitchless_after_arp();test_idle_filter();test_step_and_ratchet();test_lane_ui();test_flag_storage();
    puts("trig_lanes_test: all passed");return 0;
}
