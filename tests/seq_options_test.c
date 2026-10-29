/* SPDX-License-Identifier: GPL-3.0-only
 * Sequence option records and actual old-size catalogue objects, simulated flash. */
#include "ui_host.h"
#include <assert.h>
#include <stddef.h>
static uint8_t flash[MMS_END-MMS_BASE];
static unsigned writes;
static mmr_pattern_t pattern;
static mmr_kits_t kits;
static mmr_sounds_t sounds;
/* Frozen v1/v2 wire shape, including its original tail alignment. */
typedef struct {
    uint16_t version;uint8_t used,kit,selected,reserved;char name[12];
    int16_t g[45],timing[6][5];step_t step[6][64];
    motion_store_t motion;chain_config_v1_t song;
} old_pattern_t;
_Static_assert(sizeof(old_pattern_t)==2704,"old pattern wire layout changed");
static old_pattern_t old;
static void bounds(uint32_t off,uint32_t n)
{assert(off>=MMS_BASE&&off<=MMS_END&&n<=MMS_END-off);}
static int read_flash(void *ctx,uint32_t off,void *dst,uint32_t n)
{(void)ctx;bounds(off,n);memcpy(dst,flash+(off-MMS_BASE),n);return 0;}
static int erase_flash(void *ctx,uint32_t off)
{(void)ctx;bounds(off,MMS_SECTOR);assert(!(off%MMS_SECTOR));writes++;memset(flash+(off-MMS_BASE),255,MMS_SECTOR);return 0;}
static int program_flash(void *ctx,uint32_t off,const void *src,uint32_t n)
{
    (void)ctx;bounds(off,n);assert(n<=256&&(off%256)+n<=256);writes++;
    for(unsigned i=0;i<n;i++){uint8_t v=((const uint8_t*)src)[i],*p=&flash[off-MMS_BASE+i];assert((*p&v)==v);*p&=v;}
    return 0;
}
static mms_io io={0,read_flash,program_flash,erase_flash};
static void defaults(void)
{
    ui_host_init();memset(&chain.cfg,0,sizeof chain.cfg);memset(&mo.s,0,sizeof mo.s);
    song.seq=(pattern_options_t){0};
    for(unsigned k=0;k<NTRK;k++){
        drum_set_model(&trk[k],DM_MMGND);
        for(unsigned id=0;id<P_COUNT;id++)trk[k].p[id]=mmr_desc(DM_MMGND,id)->def;
        trk[k].model=DM_MMGND;trk[k].p[P_MODEL]=DM_MMGND;
        memset(trk[k].step,0,sizeof trk[k].step);seq_options_default(&trk[k].seq);
    }
}
static void test_options(void)
{
    defaults();seq_options_t wanted[NTRK];
    song.seq=(pattern_options_t){80,-48,3,11};
    for(unsigned k=0;k<NTRK;k++){
        seq_options_t *q=&trk[k].seq;
        for(unsigned i=0;i<8;i++)q->swing[i]=(uint8_t)(i*31+k);
        for(unsigned i=0;i<16;i++)q->arp_offset[i]=(int8_t)((int)i*8-64+(int)k);
        q->arp_mask[0]=0x55;q->arp_mask[1]=(uint8_t)k;
        q->arp_mode_order=4|(4<<3);q->arp_ojmp=7;q->arp_len=16;q->arp_flags=31;
        q->transpose=(int8_t)(48-k);q->scale_key=3*16+11;q->arp_speed=96;wanted[k]=*q;
    }
    assert(mmr_pattern_capture(&pattern,trk,song.g,&mo.s,&chain.cfg,0,0,"OPTIONS"));
    assert(pattern.version==MMR_PATTERN_VERSION);
    defaults();assert(mmr_pattern_apply(trk,song.g,&mo.s,&chain.cfg,&pattern));
    for(unsigned k=0;k<NTRK;k++)assert(!memcmp(&wanted[k],&trk[k].seq,sizeof wanted[k]));
    assert(song.seq.swing==80&&song.seq.transpose==-48&&song.seq.scale==3&&song.seq.key==11);
    seq_options_t base;seq_options_default(&base);
    for(unsigned v=0;v<256;v++){
        seq_options_t q=base;q.arp_mode_order=(uint8_t)v;assert(seq_options_valid(&q)==((v&7)<=4&&(v>>3)<=4));
        q=base;q.arp_ojmp=v;assert(seq_options_valid(&q)==(v<=7));
        q=base;q.arp_len=v;assert(seq_options_valid(&q)==(v>=1&&v<=16));
        q=base;q.arp_flags=v;assert(seq_options_valid(&q)==(v<=31));
        q=base;q.transpose=(int8_t)v;assert(seq_options_valid(&q)==(q.transpose>=-48&&q.transpose<=48));
        q=base;q.scale_key=v;assert(seq_options_valid(&q)==((v&15)<12&&(v>>4)<SEQ_NSCALES));
        q=base;q.arp_speed=v;assert(seq_options_valid(&q)==(v>=1&&v<=96));
        pattern_options_t p={0};p.swing=v;assert(pattern_options_valid(&p)==(!v||(v>=50&&v<=80)));
        p=(pattern_options_t){0};p.transpose=(int8_t)v;assert(pattern_options_valid(&p)==(p.transpose>=-48&&p.transpose<=48));
        p=(pattern_options_t){0};p.scale=v;assert(pattern_options_valid(&p)==(v<4));
        p=(pattern_options_t){0};p.key=v;assert(pattern_options_valid(&p)==(v<12));
    }
    pattern.seq[2].arp_len=0;
    song_t before=song;seq_options_t keep=trk[2].seq;
    assert(!mmr_pattern_apply(trk,song.g,&mo.s,&chain.cfg,&pattern));
    assert(!memcmp(&before,&song,sizeof song)&&!memcmp(&keep,&trk[2].seq,sizeof keep));
}
static void old_blob(unsigned version)
{
    defaults();memset(flash,255,sizeof flash);writes=0;
    assert(!library_format_authorized(io,MMS_OWNER_WAIVER));
    memset(&kits,0,sizeof kits);kits.version=MMR_VERSION;
    assert(mmr_kit_capture(&kits.kit[0],trk,"OLD KIT"));
    memset(&sounds,0,sizeof sounds);sounds.version=MMR_VERSION;
    assert(mmr_pattern_capture(&pattern,trk,song.g,&mo.s,&chain.cfg,0,0,"OLD PATTERN"));
    /* Copy the frozen fields individually; do not serialize the new struct. */
    memset(&old,0,sizeof old);old.version=version;old.used=1;
    memcpy(old.name,pattern.name,sizeof old.name);memcpy(old.g,pattern.g,sizeof old.g);
    memcpy(old.timing,pattern.timing,sizeof old.timing);old.motion=pattern.motion;memcpy(&old.song,&pattern.song,sizeof old.song);
    old.step[0][5].on=1;old.step[0][5].nt=9;old.step[0][5].rsv=version==1?255:ST_NOAMP|ST_NOPITCH;
    for(unsigned group=0;group<4;group++){
        assert(!mms_stage(&mmlib_store,8+group,&kits,sizeof kits));
        assert(!mms_stage(&mmlib_store,12+group,&sounds,sizeof sounds));
    }
    assert(!mms_stage(&mmlib_store,0,&old,sizeof old));assert(!mms_commit(&mmlib_store));
    unsigned written=writes;assert(!library_boot(io)&&library_ready()&&writes==written);
    memset(&mmlib_work,0xCC,sizeof mmlib_work);
    song.seq=(pattern_options_t){80,48,3,11};memset(&trk[0].seq,0xCC,sizeof trk[0].seq);
    assert(!library_load_pattern(0)&&writes==written);
    assert(trk[0].step[5].on&&trk[0].step[5].nt==9);
    assert(trk[0].step[5].rsv==(version==1?0:ST_NOAMP|ST_NOPITCH));
    seq_options_t want;seq_options_default(&want);
    for(unsigned k=0;k<NTRK;k++)assert(!memcmp(&want,&trk[k].seq,sizeof want));
    pattern_options_t zero={0};assert(!memcmp(&zero,&song.seq,sizeof zero));
    trk[3].seq.arp_offset[15]=-37;trk[3].seq.arp_speed=96;
    trk[3].seq.swing[7]=0x35;trk[3].seq.arp_mask[1]=0x81;
    song.seq=(pattern_options_t){73,-9,2,7};
    seq_options_t changed=trk[3].seq;pattern_options_t changed_pattern=song.seq;
    assert(!library_save_pattern(0,0));
    assert(mms_read_object(&mmlib_store,0,&pattern,sizeof pattern)==sizeof pattern&&pattern.version==MMR_PATTERN_VERSION);
    written=writes;defaults();assert(!library_boot(io));assert(!library_load_pattern(0));
    assert(writes==written&&!memcmp(&changed,&trk[3].seq,sizeof changed));
    assert(!memcmp(&changed_pattern,&song.seq,sizeof changed_pattern));
}
static void old_v3_blob(void)
{
    static mmr_pattern_v3_t v3;
    assert(mmr_pattern_capture(&pattern,trk,song.g,&mo.s,&chain.cfg,0,0,"OLD V3"));
    memset(&v3,0,sizeof v3);
    memcpy(&v3,&pattern,__builtin_offsetof(mmr_pattern_v3_t,seq));
    memcpy(v3.seq,pattern.seq,sizeof v3.seq);v3.options=pattern.options;v3.version=3;
    v3.song.count=2;v3.song.row[0]=(chain_row_t){0,3};v3.song.row[1]=(chain_row_t){0,2};
    assert(!mms_stage(&mmlib_store,0,&v3,sizeof v3)&&!mms_commit(&mmlib_store));
    unsigned written=writes;
    memset(&mmlib_work,0xCC,sizeof mmlib_work);
    assert(!library_boot(io)&&!library_load_pattern(0)&&writes==written);
    assert(chain.cfg.count==2&&chain.cfg.row[0].repeat==3);
    chain_row_options_t zero={0};
    for(unsigned i=0;i<CHAIN_ROWS;i++)assert(!memcmp(&chain.cfg.options[i],&zero,sizeof zero));
    assert(!memcmp(&song.seq,&v3.options,sizeof song.seq));
    for(unsigned k=0;k<NTRK;k++)assert(!memcmp(&trk[k].seq,&v3.seq[k],sizeof trk[k].seq));
    assert(mms_read_object(&mmlib_store,0,&v3,sizeof v3)==sizeof v3&&v3.version==3);
}
int main(void)
{
    test_options();old_blob(1);old_blob(2);old_v3_blob();
    puts("seq_options_test: all passed");return 0;
}
