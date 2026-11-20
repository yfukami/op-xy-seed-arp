// Starter note player: every note plays SHIFT semitones away.
//
// The SDK's player model (see opxy/player.hpp and ../CLAUDE.md):
// - press() takes a key and returns a Plan of up to eight tones that belong to
//   that key (its "root"); stock releases them all when the key is released.
// - update() is drained before every player step (240 per quarter note) and
//   after every press and release, at most eight calls each: it is the clock.
// - An empty Replace releases a root for good, and nothing can revive it.
#include <opxy/divide.hpp>
#include <opxy/player.hpp>

namespace newplayer {
using opxy::u32;
using opxy::player::Controls;
using opxy::player::Display;
using opxy::player::Plan;
using opxy::player::Update;

const u32 kShiftChoices = 25;  // -12..+12

struct State {
    u32 held;  // keys held, for the screen
};

// quotient16 fails when the compiler proves both operands equal (it puts them
// in one register; 100 / 100 once drew "e00%"), so keep this guard.
inline u32 divide(u32 n, u32 d) { return n == d ? 1 : opxy::divide::quotient16(n, d); }
inline u32 modulo(u32 n, u32 d) { return n - d * divide(n, d); }

inline int shift_of(const Controls& c) {
    return static_cast<int>(opxy::player::choice(c.value[0], kShiftChoices)) - 12;
}

bool press(State& s, const Controls& c, u32 note, u32 velocity, Plan& plan) {
    plan.count = 0;
    const int pitch = static_cast<int>(note) + shift_of(c);
    if (note > 127 || pitch < 0 || pitch > 127) return false;  // let the instrument play it unchanged
    plan.count = 1;
    plan.tones[0].pitch = static_cast<opxy::u8>(pitch);
    plan.tones[0].velocity = static_cast<opxy::u8>(velocity > 127 ? 127 : velocity);
    plan.tones[0].delay = 0;
    if (s.held < opxy::player::max_tones) ++s.held;
    return true;
}

bool release(State& s, u32) {
    if (s.held) --s.held;
    return false;  // stock releases this key's tones
}

bool update(State&, const Controls&, Update&) { return false; }

void reset(State& s) { s.held = 0; }

inline void append(char* out, u32 size, u32& at, const char* text) {
    while (*text && at + 1 < size) out[at++] = *text++;
    out[at] = 0;
}

inline void append_number(char* out, u32 size, u32& at, u32 value) {
    char digits[4] = {0, 0, 0, 0};
    u32 count = 0;
    if (value >= 100) digits[count++] = static_cast<char>('0' + divide(value, 100));
    if (value >= 10) digits[count++] = static_cast<char>('0' + modulo(divide(value, 10), 10));
    digits[count] = static_cast<char>('0' + modulo(value, 10));
    append(out, size, at, digits);
}

void display(const State& s, const Controls& c, Display& d) {
    const int shift = shift_of(c);
    u32 at = 0;
    if (shift) append(d.values[0], sizeof d.values[0], at, shift > 0 ? "+" : "-");
    append_number(d.values[0], sizeof d.values[0], at, static_cast<u32>(shift < 0 ? -shift : shift));
    for (u32 i = 0; i < s.held && i < opxy::player::display_dots; ++i) d.lit |= 1u << i;
    at = 0;
    append(d.text, sizeof d.text, at, s.held ? "playing" : "ready");
}
}  // namespace newplayer
