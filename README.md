# New Player

A starter for an Open-XY note player for the OP-XY (OS 1.2.1, SDK 1.16). It currently does one simple thing: each note plays SHIFT semitones away, from -12 to +12.

## Starting a real player

1. Choose an ID of at most 12 characters (the OP-XY's player menu cuts longer ones) and a display name, then run:
   `python3 tools/rename.py <id> "<Display Name>"`
2. Optionally, reserve the ID in the Open-XY catalog with a `"status": "reserved"` entry, so that nobody else takes it.
3. Replace `player.cpp`, `definition.json` and the tests with the real player, and redraw the screen in `tools/gen_panel.py`.

## Development

```sh
../open-xy/.venv/bin/python ../open-xy/sdk.py test --machine machines/newplayer --cxx /opt/homebrew/opt/llvm/bin/clang++
../open-xy/.venv/bin/python ../open-xy/sdk.py test --machine machines/newplayer --cxx /opt/homebrew/opt/llvm/bin/clang++ --firmware
```

`tests/harness.hpp` stands in for the OP-XY in host tests: it plays presses, releases and player steps, and logs every note on and off. Device checks go in [QA.md](QA.md).

## License

MIT. See [LICENSE](LICENSE).
