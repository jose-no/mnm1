/* SPDX-License-Identifier: GPL-3.0-only
 * Real-time track/pattern transpose (reference manual, TRANSPOSE pp.63–64).
 * Pattern data stays unchanged. SCALE and KEY belong to the track. */
#ifndef MNM_SEQ_TRANSPOSE_H
#define MNM_SEQ_TRANSPOSE_H
/* SCALE: --- (none), FIX (no pattern transpose), then the scales a note snaps to (bit n: n semitones above KEY).
 * MAJ and MIN are MNM-1's; DOR..WHL come with GEN (Mutable Dreams' scale set). */
static const uint16_t SEQ_SCALE_MASK[SEQ_NSCALES]={0,0,0xAB5,0x5AD,0x6AD,0x5AB,0x4A9,0x295,0x555};
static int seq_multi_pitch(const track_t *t){return (t->seq.scale_key>>4)==1?0:song.multi_pitch16;}
/* Keep scale quantization out of legacy DSP multiply scheduling windows. */
static __attribute__((noinline)) int seq_note(const track_t *t)
{
    unsigned scale=t->seq.scale_key>>4,key=(t->seq.scale_key&15u)%12u;
    int note=60+t->nt+t->seq.transpose;
    if(scale!=1)note+=song.seq.transpose+seq_multi_pitch(t)/16; /* FIX ignores pattern transpose. */
    if(note<0)note=0;
    if(note>127)note=127;
    if(scale>=2&&scale<SEQ_NSCALES){
        unsigned mask=SEQ_SCALE_MASK[scale];
        /* Manual specifies scale membership, not rounding direction. Choose
         * the nearest legal MIDI note, preferring downward on equal distance. */
        for(int distance=0;distance<128;distance++){
            int below=note-distance,above=note+distance;
            if(below>=0&&(mask&(1u<<((below+12-(int)key)%12))))return below;
            if(above<=127&&(mask&(1u<<((above+12-(int)key)%12))))return above;
        }
    }
    return note;
}
#endif
