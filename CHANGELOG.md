# Changelog

MNM-1's versions, newest first. MNM-1 is inspired by and forked from Hortator 0.23.3 by DEADACTIVE; Hortator's
own history is in its repository.

## Unreleased

- Add GEN, a phrase generator ported from Mutable Dreams 1.2 by Kid Bunka / Christoph Link (GPL-3.0-only), on two new
  SEQ pages after TRANSPOSE. GEN: GEN (SPARSE, ARP, PULSE, DRIFT writes the track's 64 steps from the pattern's seed,
  at KEY + OCT, snapped to the track's SCALE), MARB (0–100 %: off-beat notes become the root, a third, fourth or fifth;
  remembered per seed, position and 2-bar span), OCT, FLLW (the track follows the chord journey). GEN CHORD: CHRD (OFF,
  I-IV-I-V, I-VI-IV-V, I-VII-IV-I, I-III-VI-IV as scale degrees), BARS (1–8 a chord), LOCK (the variation repeats),
  UNDO (one level). REC on either page draws a new seed and writes the phrase again. Generated steps are ordinary steps;
  MARBLES and the journey change the notes as they play, never the stored steps.
- Add DOR, PHR, MIN5, MAJ5 and WHL to the track SCALE (TRANSPOSE page), after MAJ and MIN. The pattern SCALE keeps its
  four values.
- Projects become MNM9 (3680 bytes) and patterns version 5 (3066 bytes), adding GEN. MNM8 projects and version-four
  patterns load with GEN off; nothing else in them changes.

## 1.0.1 - 2026-10-27

- Give MNM-1 its own stable USB/package identity, FM-1_61000100, so firmware hubs can distinguish it from other firmware. Sound, controls and saved-data formats are unchanged.
- Publish a GitHub release with the firmware package and a directly linked 240 × 240 screen capture.

## 1.0.0 - 2026-10-26

Installed and verified on FM-1. Two bank-A saves passed CRC/readback checks across a restart, bank B remained unchanged, and the owner confirmed panel SAVE works.

- Add SONG RANGE, MIX and FLOW: per-row transpose, offset, length, mutes, tempo, counted backward loops and jumps. STOP restores the original tempo, mutes and track options. Offset follows source notes, locks, slides and swing together.
- Add SONG FILE with confirmed NEW SONG and reloadable AFTER HOURS demo. The eight-row arrangement uses kick, snare, hats, bass, melody, locks and phaser; built-in phrases occupy no user slots. Saved demo arrangements retain their built-in source references.
- Preserve MNM3–MNM7 projects and version-one/two/three pattern payloads with frozen readers. MNM8 projects occupy 3648 bytes; version-four patterns occupy 3034 bytes.
- Make SELECT navigate all pages in the current family, including appended performance and SONG pages.
- Distinguish a committed project with a failed startup-bank marker as SAVED; BOOT ERROR. Add read-only USB `storage` diagnostics with the failing operation, address, error and flash status; no protection flags are changed.
- Verify all legacy banks through simulated reboot, interrupted writes and write protection; keep the previous committed data on failed saves.
- Publish the matching 1.0 simulator and installer; remove the roadmap from the project website.

## 0.11.0 - 2026-10-26

- Add chained TRIG POS routing with cycle protection and note ownership, so an older source release cannot cut a newer independent note.
- Add MULTI ALL/SPLIT/START/TRANS modes, a selectable MIDI channel (default 7), panel keyboard input, quantized sequence changes, shared ADSR and portamento. Keep individual arpeggiators in ALL mode and handle overlapping MIDI/panel notes.
- Add two velocity and two keytrack destinations per sound, composed with parameter locks, slides and LFOs without changing stored bases. Generated ARP notes retain the last input key and velocity.
- Add independent HP/LP keytracking switches. OFF uses a fixed C4 reference; controller response, envelope/portamento curves and this reference are documented interpretations, not newly measured hardware laws.
- Preserve full performance settings in MNM7 projects and version-two full sounds/kits; older records load with disabled/default options. Loading a kit clears performance runtime, and cancelling sound audition restores the full sound.
- Make mixer level bars readable with an empty outlined background, solid level fill and the existing peak marker.
- Isolate RING's rounded multiplies from target load scheduling hazards while preserving its sample arithmetic.
- All 187 hardware stress cases have measurements across two runs. The owner confirmed manually resetting the first run; it is not counted as a firmware failure. Heavy combinations can still exceed the audio deadline.

## 0.10.0 - 2026-10-26

- Add pattern swing amount and per-track 64-step swing masks; preserve the existing swing law for older projects. Keep fractional timing and nominal SONG row boundaries.
- Add 3/4X and 3/2X speed choices without changing previous division IDs.
- Add KEY, SID and ADD arpeggiation, five playback orders, octave range/jump, trigger switches and a 16-step rhythm with note offsets. Existing ARP behavior remains available as LEGACY.
- Add track/pattern transpose, FIX and major/minor scale constraints. Scale rounding uses the nearest legal note, ties downward; octave-jump behavior is an explicitly documented interpretation pending physical comparison.
- Cache cross-LFO controls once per block with identical modulation results; device tests confirm lower overload-recovery cost. Isolate two legacy drum setup paths from target multiply scheduling hazards.
- Add controls as pages within the existing ARP and SEQ families. Store options in compact MNM6 projects and version-three expanded patterns, retaining readers for previous formats.

## 0.9.0 - 2026-10-26

- Add independent AMP, FILTER and LFO trig lanes, trigless notes, note-off, pitchless and locks-only steps. Ratchets retain their original event flags.
- STEP knob 2 selects ALL/AMP/FILTER/LFO; holding a step and turning SELECT chooses its trig type. Existing probability, ratchet, accent and parameter-slide knobs remain available.
- Keep the last sounding note for pitchless events after arpeggiation, and advance silent calibrated filter envelopes without allocating an audible voice.
- Save step flags in MNM5 projects and version-two patterns, read older formats as ALL, and restore parameter-slide on project load.

## 0.8.0 - 2026-10-26

- Add eight patterns, eight independent kits and 128 full user sounds, including calibrated parameters. Keep legacy storage until an explicit migration.
- Add KIT save/load/name controls and pattern slots E–H within the existing interface. Preserve kit names when saving patterns; restore the last saved slot after restart.
- Cache all eight SONG source patterns before playback; row changes perform no flash reads. Refuse kits exceeding existing model limits before changing the live sound.
- Expose explicit `library migrate-waived` in the USB console. It replaces sample slots 2 and 3, preserves slot 1 and the old project/user records, and never runs automatically at boot.
- Verify migration, atomic writes, interrupted transactions, full-sound audition rollback, kit/pattern UI and read failures with host sanitizers. Device timing and listening checks remain pending.

## 0.7.0 - 2026-10-26

- Add 19 separately selectable calibrated machines from MONO2K, preserving legacy model IDs and algorithms.
- Port the complete AMP, filter, EQ, distortion, VOL, stereo pan/LEVEL and modulation-effect chain, with raw parameter values and fractional LFO modulation.
- Import 24 current and 96 historical complete factory sounds. Audition/cancel restores the full sound. Seven banks contain 330 factory presets in total.
- Restore STRNG, PIPE, CHORD and MODAL FX; add tape, adaptive crunch, hiss/dust, bus drive and stereo reverb. Calibrated delays retain stereo and share the existing track buffers.
- Add source LFO target/phase controls, DESTROY and MOD FX pages, independent sound VOL, held-key arpeggiation and chromatic MIDI input on channels 1–6 for calibrated tracks.
- Migrate saved MNM3 projects to MNM4 without rewriting flash automatically. Legacy user slots remain intact; full calibrated user saves require the expanded store.
- Limit calibrated preview work per LCD frame. Add source differential tests, storage power-cut tests and a target multiply-scheduling regression check.
- Add a second-core worker for independent calibrated synth renders; routing, dependent SID/FX tracks and mixing retain their original order. Serial/threaded output and state match in sanitizer tests; hardware timing remains unverified.
- VOX consonant and vowel-switch controls remain unimplemented in the source and display `n/a`. Calibration evidence comes from an emulator, not new measurements of physical hardware.


## 0.6.1 - 2026-10-26

- ORGAN and ROBOT: adjusted their sound knobs to bring both within 6 dB of the synth bank median. Preset ids and saved projects stay compatible.
- LOCK: hold a step and OCT-, then turn a knob to clear only its lock. Tap OCT- while holding a step to clear all its locks on release.
- Manual: the page table now includes the preset list and its controls.

## 0.6.0 - 2026-10-25

- PRESETS: a new bank, MNM, first in the list: 55 presets for MNM-1's machines. SWSAW (SUPERSAW HOOVER SAWBASS
  TRANCE WARMPAD), SWPLS (PWMLEAD SQRBASS THINPLS PWMPAD CHORUSPL), SID (SIDPULS SIDSYNC SIDRING SIDBASS CHIPTRI
  SIDNOISE FIFTHS), FM+ (FMKEYS EPIANO FMBASS TUBEBELL METALFM FMBRASS), DPRO (DPROWAVE ORGAN VOXSCAN FOLDBASS
  SYNCSWP SCANPAD), GND (GNDSINE GNDKICK GNDTOM GNDHAT ZAP DIGINOIS SUBSINE), VO-6 (VOX AH CHOIR WHISPER ROBOT OOH
  LOWVOX) and the FX machines (PLATE BIGROOM GATEDREV, CHORUS WIDECHO, FLANGER JETFLNG COMBRING, PHASER SLOWPHS,
  COMPRESS LIMITER, RINGMOD). A preset sets the machine and its 8 SYNTH knobs. Each machine's first preset is its
  default sound. The PRESETS knob steps through them; EDIT held opens the list.
- SID: RING on with the TRI wave played far out of range (a loud, broken sound); the ring now turns the triangle over
  within its range.

## 0.5.1 - 2026-10-25

- The PRESETS bank of the TRX and EFM voices is named MD. ABOUT's credits are shorter.
- Web simulator: the firmware compiled to WebAssembly, on the project's page, with MNM-1 demos (four projects built on
  the power-on kit: SID lead, DPRO pad, VO-6 voice, FM+ bass) and a reel of the 8-knob pages.

## 0.5.0 - 2026-10-25

- Parameter locks: a step with a trig or a lock first puts the locked knobs back to the kit's value, then sets its
  own locks. A locked value lasts until the next trig, and an unlocked trig plays the kit's sound. A lock on a step
  with no trig applies at that step. Up to 240 locks per project.
- Parameter slide: on a SLIDE step (STEP page, hold a step, KNOB 4) each locked knob glides to its next lock on the
  track. Unlocked trigs in between don't stop it; a knob locked only once doesn't slide.
- LOCK mode: on STEP, hold a step and press EDIT, ENV, FX or LFO. The 8-knob page opens with the keys still the
  steps. Hold a step and turn a knob to lock it there; OCT- clears the held step's locks; SEQ leaves.
- Projects are "MNM3"; older records load as empty.

## 0.4.0 - 2026-10-25

- FILTER (ENV): a resonant high-pass at BASE into a low-pass at BASE + WDTH, with separate HPQ and LPQ, tracking the
  note (BASE 0: two octaves under it, 8 steps an octave), and an AD envelope adding BOFS to BASE and WOFS to WDTH.
- EFFECTS (FX): EQF EQG SRR DTIM / DSND DFB DBAS DWID. A peaking EQ (+-36 dB) and a stereo tape delay on every
  track: DTIM in 1/256 notes, DSND's sign keeps the image or ping-pongs, DFB above 64 grows into the clip.
- Three LFOs per track: PAGE DEST TRIG WAVE / MULT SPD INTL DPTH. Any knob of SYN, AMP, FLT or EFX, another LFO's
  knobs, or PTCH (vibrato to 16 octaves). Tempo-locked, FREE TRIG HOLD ONE HALF, eleven waves, INTL.
- The REVERB and CHORUS sends move to TRACK; SLICER is its own page. Projects are "MNM2".

## 0.3.0 - 2026-10-25

- 8-knob pages: two rows of 4 on the FM-1's four knobs, a tab strip (SYN AMP FLT EFX LF1 LF2 LF3).
  Pressing a page button again steps through its rows and pages. SELECT picks the track's machine on these pages.

## 0.2.0 - 2026-10-25

- Six tracks. FX machines (THRU REVERB CHORUS DYNAMIX RINGMOD PHASER FLANGER) fed by the
  previous track (NEIB) or a bus (AB, CD, EF), as an insert or a send and return; each track's OUT bus.
- The keys play the selected track's notes at power-on (SCL turns KEYS off).
- Power-on kit: GND kick, FM+ snare through a FLANGER comb, SWSAW bass, SWPLS arp into a REVERB send.

## 0.1.0 - 2026-10-24

- Seven synth machines (SWSAW SWPLS SID FM+ DPRO GND VO-6) with an AMP envelope (ATK HOLD DEC REL)
  and portamento, a dual HP/LP filter, per-track SRR and an arpeggiator per track. MNM theme and name.
