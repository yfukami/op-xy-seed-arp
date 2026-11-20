# Device checklist

Run every check on one OP-XY (OS 1.2.1), top to bottom, in one project, on the exact commit you will publish. Host tests cannot answer these questions. Add the player's own checks under "Musical features".

**Setup:** one track with a simple sustaining synth patch, with this player as its player.

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

13. (Add the player's own checks here.)

## Results

Not yet run.
