/* SPDX-License-Identifier: GPL-3.0-only
 * Virtual sequencer controls; selectors are UI state, never persisted. */
#ifndef SEQ_OPTIONS_UI_H
#define SEQ_OPTIONS_UI_H
enum {
 SO_MODE,SO_ORDER,SO_SPEED,SO_RANGE,SO_OJMP,SO_LENGTH,SO_AMP,SO_FLT,SO_LFO,
 SO_RSTEP,SO_RON,SO_ROFF,SO_SWING,SO_SSTEP,SO_SON,SO_TRANS,SO_SCALE,SO_KEY,
 SO_PTRANS,SO_PSCALE,SO_PKEY,SO_COUNT
};
static uint8_t seqopt_rhythm_step,seqopt_swing_step;
static const char *const SO_MODES[]={"LEGACY","KEY","SID","ADD","OFF"};
static const char *const SO_ORDERS[]={"TRUE","UP","DOWN","CYCLE","RND"};
static const char *const SO_SCALES[]={"---","FIX","MAJ","MIN","DOR","PHR","MIN5","MAJ5","WHL"};
static const char *const SO_PSCALES[]={"---","FIX","MAJ","MIN"};   /* (the pattern's: stored as before) */
static const char *const SO_KEYS[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const param_desc_t SO_DESC[SO_COUNT]={
 [SO_MODE]=PE("MODE",SO_MODES,0),[SO_ORDER]=PE("PLAY",SO_ORDERS,0),
 [SO_SPEED]=PD("SPD",F_INT,1,96,6),[SO_RANGE]=PD("RNGE",F_INT,1,4,1),
 [SO_OJMP]=PD("OJMP",F_INT,0,7,0),[SO_LENGTH]=PD("LEN",F_INT,1,16,8),
 [SO_AMP]=PD("AMP",F_ONOFF,0,1,1),[SO_FLT]=PD("FLT",F_ONOFF,0,1,1),[SO_LFO]=PD("LFO",F_ONOFF,0,1,1),
 [SO_RSTEP]=PD("STEP",F_INT,1,16,1),[SO_RON]=PD("ON",F_ONOFF,0,1,1),[SO_ROFF]=PD("OFFS",F_INT,-128,127,0),
 [SO_SWING]=PD("SWING",F_INT,0,80,0),[SO_SSTEP]=PD("STEP",F_INT,1,64,1),[SO_SON]=PD("ON",F_ONOFF,0,1,0),
 [SO_TRANS]=PD("TRANS",F_INT,-48,48,0),[SO_SCALE]=PE("SCALE",SO_SCALES,0),[SO_KEY]=PE("KEY",SO_KEYS,0),
 [SO_PTRANS]=PD("PAT",F_INT,-48,48,0),[SO_PSCALE]=PE("SCALE",SO_PSCALES,0),[SO_PKEY]=PE("KEY",SO_KEYS,0)
};
static int seqopt_get(const track_t *t,const pattern_options_t *p,unsigned id)
{
 const seq_options_t*s=&t->seq;
 switch(id){
 case SO_MODE:return s->arp_mode_order&7;case SO_ORDER:return s->arp_mode_order>>3;
 case SO_SPEED:return s->arp_speed;case SO_RANGE:return 1+((s->arp_flags>>3)&3);
 case SO_OJMP:return s->arp_ojmp;case SO_LENGTH:return s->arp_len;
 case SO_AMP:case SO_FLT:case SO_LFO:return !(s->arp_flags&(1u<<(id-SO_AMP)));
 case SO_RSTEP:return seqopt_rhythm_step+1;case SO_RON:return (s->arp_mask[seqopt_rhythm_step/8]>>(seqopt_rhythm_step%8))&1;
 case SO_ROFF:return s->arp_offset[seqopt_rhythm_step];case SO_SWING:return p->swing;
 case SO_SSTEP:return seqopt_swing_step+1;case SO_SON:return (s->swing[seqopt_swing_step/8]>>(seqopt_swing_step%8))&1;
 case SO_TRANS:return s->transpose;case SO_SCALE:return s->scale_key>>4;case SO_KEY:return s->scale_key&15;
 case SO_PTRANS:return p->transpose;case SO_PSCALE:return p->scale;case SO_PKEY:return p->key;
 default:return 0;
 }
}
/* Invalid values are rejected; the knob caller handles the 0 -> 50 swing gap. */
static int seqopt_set(track_t *t,pattern_options_t *p,unsigned id,int value)
{
 if(id>=SO_COUNT||value<SO_DESC[id].min||value>SO_DESC[id].max||
    (id==SO_SWING&&value&&value<50)||seqopt_get(t,p,id)==value)return 0;
 seq_options_t*s=&t->seq;
 switch(id){
 case SO_MODE:s->arp_mode_order=(uint8_t)((s->arp_mode_order&~7u)|(unsigned)value);break;
 case SO_ORDER:s->arp_mode_order=(uint8_t)((s->arp_mode_order&7u)|((unsigned)value<<3));break;
 case SO_SPEED:s->arp_speed=(uint8_t)value;break;
 case SO_RANGE:s->arp_flags=(uint8_t)((s->arp_flags&~24u)|((unsigned)(value-1)<<3));break;
 case SO_OJMP:s->arp_ojmp=(uint8_t)value;break;case SO_LENGTH:s->arp_len=(uint8_t)value;break;
 case SO_AMP:case SO_FLT:case SO_LFO:{unsigned bit=1u<<(id-SO_AMP);s->arp_flags=(uint8_t)((s->arp_flags&~bit)|(value?0:bit));break;}
 case SO_RSTEP:seqopt_rhythm_step=(uint8_t)(value-1);break;
 case SO_RON:{unsigned bit=1u<<(seqopt_rhythm_step%8);uint8_t*b=&s->arp_mask[seqopt_rhythm_step/8];*b=(uint8_t)((*b&~bit)|(value?bit:0));break;}
 case SO_ROFF:s->arp_offset[seqopt_rhythm_step]=(int8_t)value;break;
 case SO_SWING:p->swing=(uint8_t)value;break;case SO_SSTEP:seqopt_swing_step=(uint8_t)(value-1);break;
 case SO_SON:{unsigned bit=1u<<(seqopt_swing_step%8);uint8_t*b=&s->swing[seqopt_swing_step/8];*b=(uint8_t)((*b&~bit)|(value?bit:0));break;}
 case SO_TRANS:s->transpose=(int8_t)value;break;
 case SO_SCALE:s->scale_key=(uint8_t)((s->scale_key&15u)|((unsigned)value<<4));break;
 case SO_KEY:s->scale_key=(uint8_t)((s->scale_key&240u)|(unsigned)value);break;
 case SO_PTRANS:p->transpose=(int8_t)value;break;case SO_PSCALE:p->scale=(uint8_t)value;break;case SO_PKEY:p->key=(uint8_t)value;break;
 default:return 0;
 }
 return 1;
}
#endif
