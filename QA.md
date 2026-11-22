# Device checklist

Run every check on one OP-XY (OS 1.2.1), top to bottom, in one project, on the exact commit you will publish. Host tests cannot answer these questions. Add the player's own checks under "Musical features".

**Setup:** one track with a simple sustaining synth patch, with Seed Seq as its player, at its defaults: SEED 0, BANK 0, DENSITY 60%, STYLE up, LENGTH 16, RATE 1/16, GATE 50%, RANGE 1. Hold or step-sequence C, E, G unless a check says otherwise.

## Core timing

1. **Transport stopped.** Hold a key with the transport stopped. Pass: the player works as it does while playing.
2. **Transport playing.** Press play and hold a key against a drum at 80, 120 and 160 BPM. Pass: the timing stays locked.
3. **Long hold.** Hold three keys for five minutes. Pass: no stray or stuck notes, and no clicks.

## Notes stop cleanly

4. **Releasing keys one at a time.** Pass: each released key's notes stop, and nothing hangs.
5. **Stopping a sequence.** Sequence notes, press play, then stop mid-note. Pass: silence, nothing hangs.
6. **Switching track and project while holding keys.** Pass: nothing hangs, and the next key starts cleanly.
7. **Nine keys.** Pass: eight play, and the ninth has no effect, including on release.

## Controls and screen

8. **Full range of every control.** Pass: one value per detent, and no caption touches the next column.
9. **Page switching,** if the player has two pages. Pass: the header switches, and all values read correctly.
10. **Screen artwork.** Pass: every shape shows, below the header and legible.
11. **Saving.** Set every control to an unusual value, save, switch project and back. Pass: all values return.
12. **Name.** Pass: the player menu shows the ID in full.

## Musical features

13. **A pattern.** Pass: a rhythm that repeats every 16 steps, starting on the downbeat, playing C E G upwards on its played steps. The strip matches the rhythm you hear.
14. **Seeds and banks.** Turn SEED, then BANK. Pass: the strip and the text line (for example `seed 3.42`) change at once, even while stopped. Each gives a different rhythm, and going back gives the same one again.
15. **Density.** Sweep DENSITY from 0% to 100%, then on through 95%* to 0%*. Pass: 0% is silent, steps are only added as it rises, and 100% plays every step. With STYLE up at 60%, every note of C E G comes through in turn. At 60%* the rhythm is the same, but some notes are skipped, so the melody matches the 100% version with gaps.
16. **Styles.** Try every STYLE. Pass: the rhythm stays the same while the note order changes. Random styles repeat each loop. Chord and chd/1 on a monophonic instrument keep the lowest note.
17. **Transpose.** Hold or sequence a different chord. Pass: the same rhythm and pattern, on the new notes.
18. **Range and gate.** RANGE 3, then GATE 10% and 100%. Pass: the pattern spreads over three octaves, and notes get short or long.
19. **Saving.** Save the project with settings you like, then reload it. Pass: all eight settings return and the same pattern plays.
20. **Length and rates.** LENGTH 5 and 24, and every RATE. Pass: the loop wraps at LENGTH, and with LENGTH over 16 the strip follows the playhead into the second half.
21. **Hand-played chords.** Play C-E-G together by hand several times. Pass: no stab, and the same start as a sequenced chord.

## Results

Not yet run.
