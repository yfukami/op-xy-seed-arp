# Seed Seq

An Open-XY note player for the OP-XY (OS 1.2.1, SDK 1.16): a seeded pattern sequencer. The notes you hold or sequence, such as a chord or a scale, are its pitches. BANK and SEED choose a rhythm, and STYLE the order the notes play in.

The same seed and settings always give the same pattern, so a pattern you like is saved with the project like any other setting. Hold different notes and the same pattern plays them.

## Controls

Press any encoder to switch pages. All eight are saved with the project, and since the pattern is worked out from them and the held keys, it comes back exactly when the project reloads.

| Encoder | Page 1 | Page 2 |
|---|---|---|
| 1 | SEED: 0 to 254 | LENGTH: 1 to 32 steps |
| 2 | BANK: 0 to 254, each with its own 255 seeds | RATE: 1/2t to 1/32t, triplets included |
| 3 | DENSITY: how many steps play, 0% to 100%, then 95%* to 0%* | GATE: 10% to 100% |
| 4 | STYLE: the order notes play in | RANGE: 1 to 3 octaves above the held notes |

- **Rhythm:** BANK and SEED choose the rhythm, one of 65,025, and DENSITY how many of its steps play. Step 1 always plays, so the pattern has a downbeat. Raising DENSITY only adds steps and never moves the ones already playing.
- **STYLE** picks the order notes play in, from the same patterns as Chain Arp: up, down, up/dn, dn/up, updn+, dnup+, order, conv, divrg, pinky, thumb, the four stairs (stu+, stu-, std-, std+), shuf, rand, drunk, chord and chd/1.
  - **Next note:** each played step takes the pattern's next note, and every loop starts the pattern again.
  - **Starred DENSITY skips:** with DENSITY on a `*` value, the pattern moves on every step instead, so a rest skips the note it would have played. Each played step then plays exactly the note it would at 100%. Unstarred, the whole pattern comes through however sparse the rhythm.
  - **Random styles:** shuf, rand and drunk draw from the seed, so they repeat each loop and after a reload.
- **Chords on a monophonic instrument:** chord and chd/1 send the lowest note last, so an instrument that can play only one note keeps the lowest.
- **Starting:** after you release every key, the next key starts the pattern again from step 1. Keys that arrive together, such as a sequenced chord, start at once. A key that arrives on its own waits about 15 ms for the rest of a hand-played chord. The steps still count from the first key.

## Screen

- **Strip:** 16 cells showing the pattern's rhythm. It changes as soon as you turn BANK, SEED, DENSITY or LENGTH. With LENGTH over 16, the strip shows whichever half is playing.
- **Playhead:** a tick under the strip.
- **Text line:** the bank and seed, such as `seed 3.42` when stopped, or the playing note with them, such as `E4  #3.42`.

## Development

```sh
../open-xy/.venv/bin/python ../open-xy/sdk.py test --machine machines/seed_seq --cxx /opt/homebrew/opt/llvm/bin/clang++
../open-xy/.venv/bin/python ../open-xy/sdk.py test --machine machines/seed_seq --cxx /opt/homebrew/opt/llvm/bin/clang++ --firmware
```

`tests/harness.hpp` stands in for the OP-XY in host tests. To review the screen, run `python3 tools/gen_panel.py --state 0x95a3,6 --out /tmp/strip.svg`. Device checks are in [QA.md](QA.md).

## License

MIT. See [LICENSE](LICENSE).
