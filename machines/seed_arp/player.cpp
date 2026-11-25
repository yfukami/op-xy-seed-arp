// Seed Arp: a seeded pattern sequencer. The held keys (a scale or a chord) are
// the pitches. BANK and SEED choose one of 65,025 rhythms, DENSITY how many of
// its steps play, and STYLE the order the notes play in: each played step takes
// the pattern's next note, and the pattern starts again with every loop. The
// result is a pure function of the controls and the held keys, so the project
// saves it, the screen shows it as soon as a control turns, and it follows
// whatever keys are held. The random styles draw from the seed, so they repeat.
//
// As in the device-tested players: the sounding note lives in the chord record
// of a held key (its carrier); a held key carrying nothing keeps a far-future
// parked tone, because an empty Replace would release the root for good; and
// update() is drained before every player step and after every press and
// release, so press and release mark their drain and the others are the clock.
#include <opxy/divide.hpp>
#include <opxy/player.hpp>

namespace seed_arp {
using opxy::u8;
using opxy::u16;
using opxy::u32;
using opxy::player::Controls;
using opxy::player::Display;
using opxy::player::Plan;
using opxy::player::Tone;
using opxy::player::Update;

const u32 kSlots = opxy::player::max_tones;  // the SDK holds at most eight roots
const u32 kSteps = 32;
const u32 kMaxRange = 4;
const u32 kPool = kSlots * kMaxRange;
const u32 kDrainLimit = 8;             // the SDK's updates per drain
const u16 kParkedDelay = 65535;        // player steps before a parked tone would sound
const u32 kRearmInterval = 32768;      // parked tones are renewed well before that
const u32 kRates = 9;
const u32 kSeeds = 255;
const u32 kDensities = 41;             // DENSITY: 0..100%, then 95%*..0%* (skipping)
const u32 kGates = 19;                 // GATE: 10..100% in steps of 5
const u32 kDefaultChordWindow = 7;     // player steps, about 15 ms at 120 BPM
const u32 kStartWord = kSteps;         // the word seeding the random styles each loop

// The patterns, as in Chain Arp. STYLE offers every one but Off.
enum Pattern {
    Off, Up, Down, UpDown, DownUp, UpDownBoth, DownUpBoth, Order, Converge, Diverge,
    Pinky, Thumb, Stair, StairUpFalling, StairDownFalling, StairDownRising,
    Shuffle, Random, Drunk, Chord, ChordThenUp
};
const u32 kPatterns = 21;
const u32 kStyles = kPatterns - 1;
const u32 kChordHit = 0xff;  // next_index's answer for a step that plays every held note

struct Key {
    u8 root, velocity;
    u32 order;
};

// The random styles' memory: SHUF's order, DRUNK's place and the generator.
struct Walker {
    u8 perm[kPool];
    u32 perm_count, last, drunk, rng;
    u8 drunk_placed;
};

struct Params {
    u32 seed, bank, pattern, density, style, length, rate, gate, range;
    bool skip;  // DENSITY's starred half: rests skip the notes they would have played
};

struct State {
    Key keys[kSlots];
    u8 sorted_pitch[kPool], sorted_slot[kPool];    // low to high, across RANGE
    u8 ordered_pitch[kPool], ordered_slot[kPool];  // in press order, octave by octave
    u8 chord_pitch[kSlots], chord_velocity[kSlots]; // a chord step's notes, highest first
    Walker walker;
    u32 pool_count, pool_range, order_counter, tick, gate_left, rearm, start_wait, notes_played;
    u8 pool_dirty, active, dirty, event_pending, draining, drain_calls, started, waiting;
    u8 current, carrier, pitch, velocity, chord_count, has_note;  // the carrier is a slot + 1, 0 when silent
};

inline u32 smaller(u32 a, u32 b) { return a < b ? a : b; }
// quotient16's divide steps fail when both operands share a register, which the
// compiler chooses when it can prove them equal (100 / 100 gave the "e" in "e00%").
inline u32 divide(u32 n, u32 d) { return n == d ? 1 : opxy::divide::quotient16(n, d); }
inline u32 modulo(u32 n, u32 d) { return n - d * divide(n, d); }

inline Params read_params(const Controls& c) {
    using opxy::player::choice;
    static const u16 rates[kRates] = {320, 240, 160, 120, 80, 60, 40, 30, 20};
    Params p;
    p.seed = choice(c.value[0], kSeeds);
    p.bank = choice(c.value[1], kSeeds);
    p.pattern = p.bank * kSeeds + p.seed;
    // 0..100%, then on past 100% down through 95%*..0%*, where rests skip notes.
    const u32 density = choice(c.value[2], kDensities);
    p.skip = density > 20;
    p.density = 5 * (p.skip ? 40 - density : density);
    p.style = 1 + choice(c.value[3], kStyles);
    p.length = 1 + choice(c.value[4], kSteps);
    p.rate = rates[choice(c.value[5], kRates)];
    p.gate = 10 + 5 * choice(c.value[6], kGates);
    p.range = 1 + choice(c.value[7], kMaxRange);
    return p;
}

inline u32 mix(u32 x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

// A step's word, from the pattern (BANK and SEED) and the step.
inline u32 word(u32 pattern, u32 step) { return mix((pattern + 1) * 0x9e3779b9u ^ (step + 1) * 0x85ebca6bu); }

// True with probability percent / 100, decided by one random byte.
inline bool passes(u32 byte, u32 percent) {
    if (percent >= 100) return true;
    return (((byte & 0xff) * 100) >> 8) < percent;
}

// Whether a step plays. Each step has a fixed threshold, so raising DENSITY
// only adds steps; step 1 always plays, to anchor the downbeat.
inline bool plays(const Params& p, u32 step) {
    if (!p.density) return false;
    return !step || passes(word(p.pattern, step), p.density);
}

// ---- Patterns: pure functions of the note count and N, the expanded note count ----

inline u32 cycle_length(u32 pattern, u32 n) {
    if (!n) return 0;
    if (n == 1) return 1;
    switch (pattern) {
    case UpDown:
    case DownUp: return 2 * n - 2;
    case UpDownBoth:
    case DownUpBoth: return 2 * n;
    case Pinky:
    case Thumb: return 2 * (n - 1);
    case Stair:
    case StairUpFalling:
    case StairDownFalling:
    case StairDownRising: return n < 3 ? n : 3 * (n - 2);
    case ChordThenUp: return 1 + n;
    default: return n;
    }
}

inline u32 next_random(u32& rng) {
    u32 x = rng ? rng : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng = x;
    return x;
}

inline u32 random_below(u32& rng, u32 n) { return ((next_random(rng) >> 16) * n) >> 16; }

inline u32 converge(u32 step, u32 n) { return (step & 1) ? n - 1 - (step >> 1) : (step >> 1); }

// SHUF: every note once per cycle, never starting on the note that ended the last one.
inline void shuffle(Walker& w, u32 n) {
    for (u32 i = 0; i < n; ++i) w.perm[i] = static_cast<u8>(i);
    for (u32 i = n - 1; i > 0; --i) {
        const u32 j = random_below(w.rng, i + 1);
        const u8 swap = w.perm[i];
        w.perm[i] = w.perm[j];
        w.perm[j] = swap;
    }
    if (n > 1 && w.perm[0] == w.last) {
        const u8 swap = w.perm[0];
        w.perm[0] = w.perm[1];
        w.perm[1] = swap;
    }
    w.perm_count = n;
}

// The 0-based index into the note list at `step` of a cycle, or kChordHit.
inline u32 next_index(u32 pattern, u32 step, u32 n, Walker& w) {
    if (n <= 1) return 0;
    switch (pattern) {
    case Up:
    case Order: return step;
    case Down: return n - 1 - step;
    case UpDown: return step < n ? step : 2 * n - 2 - step;
    case DownUp: return n - 1 - (step < n ? step : 2 * n - 2 - step);
    case UpDownBoth: return step < n ? step : 2 * n - 1 - step;
    case DownUpBoth: return step < n ? n - 1 - step : step - n;
    case Converge: return converge(step, n);
    case Diverge: return converge(n - 1 - step, n);
    case Pinky: return (step & 1) ? n - 1 : step >> 1;
    case Thumb: return (step & 1) ? (step + 1) >> 1 : 0;
    // The stairs: groups of three, the staircase climbing up or down one note per
    // group, the notes in each group rising or falling. Under 3 notes: UP or DOWN.
    case Stair: return n < 3 ? step : divide(step, 3) + modulo(step, 3);
    case StairUpFalling: return n < 3 ? step : divide(step, 3) + 2 - modulo(step, 3);
    case StairDownFalling: return n < 3 ? n - 1 - step : n - 1 - divide(step, 3) - modulo(step, 3);
    case StairDownRising: return n < 3 ? n - 1 - step : n - 3 - divide(step, 3) + modulo(step, 3);
    case Shuffle: {
        if (!step || w.perm_count != n) shuffle(w, n);
        w.last = w.perm[step];
        return w.last;
    }
    case Random: return random_below(w.rng, n);
    case Drunk: {
        if (!w.drunk_placed || w.drunk >= n) {
            w.drunk = random_below(w.rng, n);
            w.drunk_placed = 1;
        } else if (next_random(w.rng) & 0x10000u) {
            w.drunk = w.drunk + 1 < n ? w.drunk + 1 : w.drunk - 1;
        } else {
            w.drunk = w.drunk ? w.drunk - 1 : 1;
        }
        return w.drunk;
    }
    case Chord: return kChordHit;
    case ChordThenUp: return step ? step - 1 : kChordHit;
    default: return 0;
    }
}

// ---- Keys and the note lists ----

inline u32 count_bits(u32 mask) {
    u32 count = 0;
    for (; mask; mask &= mask - 1) ++count;
    return count;
}

inline u32 find_slot(const State& s, u32 note) {
    for (u32 i = 0; i < kSlots; ++i)
        if ((s.active >> i & 1) && s.keys[i].root == note) return i;
    return kSlots;
}

inline void insert(u8* pitches, u8* slots, u32& count, u32 pitch, u32 slot, bool sorted) {
    for (u32 i = 0; i < count; ++i)
        if (pitches[i] == pitch) return;
    u32 at = count++;
    if (sorted)
        for (; at && pitches[at - 1] > pitch; --at) {
            pitches[at] = pitches[at - 1];
            slots[at] = slots[at - 1];
        }
    pitches[at] = static_cast<u8>(pitch);
    slots[at] = static_cast<u8>(slot);
}

// Both note lists, across RANGE octaves; bounded by kPool.
inline void rebuild_pool(State& s, u32 range) {
    u8 ordered[kSlots];
    u32 keys = 0;
    for (u32 i = 0; i < kSlots; ++i) {
        if (!(s.active >> i & 1)) continue;
        u32 at = keys++;
        for (; at && s.keys[ordered[at - 1]].order > s.keys[i].order; --at) ordered[at] = ordered[at - 1];
        ordered[at] = static_cast<u8>(i);
    }
    u32 sorted_count = 0, ordered_count = 0;
    for (u32 octave = 0; octave < range; ++octave)
        for (u32 i = 0; i < keys; ++i) {
            const u32 pitch = s.keys[ordered[i]].root + 12 * octave;
            if (pitch > 127) continue;
            insert(s.sorted_pitch, s.sorted_slot, sorted_count, pitch, ordered[i], true);
            insert(s.ordered_pitch, s.ordered_slot, ordered_count, pitch, ordered[i], false);
        }
    s.pool_count = sorted_count;
    s.pool_range = range;
    s.pool_dirty = 0;
}

// ---- Playing ----

inline void mark(State& s, u32 slot) { s.dirty |= static_cast<u8>(1u << slot); }

inline void stop_note(State& s) {
    if (s.carrier) mark(s, s.carrier - 1);
    s.carrier = 0;
    s.chord_count = 0;
    s.gate_left = 0;
}

inline void start_note(State& s, u32 slot, u32 pitch) {
    s.carrier = static_cast<u8>(slot + 1);
    s.pitch = static_cast<u8>(pitch);
    s.velocity = s.keys[slot].velocity;
    mark(s, slot);
}

// A chord step: every held key at `octave`, from one record, highest note first
// and lowest last, so a monophonic instrument keeps the lowest note.
inline void start_chord(State& s, u32 octave) {
    u32 lowest = kSlots;
    s.chord_count = 0;
    for (u32 i = 0; i < kSlots; ++i) {
        const u32 pitch = s.keys[i].root + 12 * octave;
        if (!(s.active >> i & 1) || pitch > 127) continue;
        u32 at = s.chord_count++;
        for (; at && s.chord_pitch[at - 1] < pitch; --at) {
            s.chord_pitch[at] = s.chord_pitch[at - 1];
            s.chord_velocity[at] = s.chord_velocity[at - 1];
        }
        s.chord_pitch[at] = static_cast<u8>(pitch);
        s.chord_velocity[at] = s.keys[i].velocity;
        if (lowest == kSlots || s.keys[i].root < s.keys[lowest].root) lowest = i;
    }
    if (lowest == kSlots) return;
    s.carrier = static_cast<u8>(lowest + 1);
    s.pitch = s.chord_pitch[s.chord_count - 1];
    mark(s, lowest);
}

inline void enter_step(State& s, const Params& p, u32 step) {
    if (s.pool_dirty || s.pool_range != p.range) rebuild_pool(s, p.range);
    if (!step) {  // every loop starts the pattern, and its randomness, again
        s.notes_played = 0;
        s.walker.rng = word(p.pattern, kStartWord) | 1;
        s.walker.perm_count = 0;
        s.walker.last = 0xff;
        s.walker.drunk_placed = 0;
    }
    const u32 n = s.pool_count;
    if (!n) return;
    const bool sounds = plays(p, step);
    // Each played step takes the pattern's next note. With DENSITY starred the
    // pattern moves on every step instead, so a rest skips the note it would
    // have played: every played step plays the note it would at 100%.
    if (!sounds && !p.skip) return;
    const u32 length = cycle_length(p.style, n);
    const u32 at = modulo(s.notes_played++, length);
    const u32 index = next_index(p.style, at, n, s.walker);
    if (!sounds) return;
    stop_note(s);
    if (index == kChordHit) {
        start_chord(s, p.style == Chord ? modulo(at, p.range) : 0);  // CHORD climbs RANGE, one octave per hit
    } else {
        const bool ordered = p.style == Order;
        start_note(s, ordered ? s.ordered_slot[index] : s.sorted_slot[index],
                   ordered ? s.ordered_pitch[index] : s.sorted_pitch[index]);
    }
    if (!s.carrier) return;
    s.has_note = 1;
    const u32 gate = divide(p.rate * p.gate, 100);
    s.gate_left = gate ? gate : 1;
}

// How long a key that arrives alone waits for the rest of a hand-played chord:
// about 15 ms (a player step lasts 2,500,000 / bpm_x10 microseconds), always
// under half a step.
inline u32 chord_window(u32 bpm_x10, u32 rate) {
    u32 window = bpm_x10 ? divide(bpm_x10 * 3, 500) : kDefaultChordWindow;
    if (window < 1) window = 1;
    return smaller(window, rate / 2);
}

inline void clock_step(State& s, const Controls& c) {
    if (!s.active) return;
    const Params p = read_params(c);
    if (!s.started) {  // a new phrase starts at step 1
        s.started = 1;
        s.current = 0;
        s.tick = 0;
        // Keys that arrived together (a sequenced chord) play at once. A key that
        // arrived alone waits about 15 ms for the rest of a hand-played chord, so
        // the pattern starts from every key in it. The steps still count from the
        // first key.
        if (count_bits(s.active) >= 2) {
            enter_step(s, p, 0);
        } else {
            s.waiting = 1;
            s.start_wait = chord_window(c.bpm_x10, p.rate);
        }
        return;
    }
    if (s.waiting) {
        ++s.tick;
        if (--s.start_wait) return;
        s.waiting = 0;
        enter_step(s, p, 0);
        return;
    }
    if (s.gate_left && --s.gate_left == 0) stop_note(s);
    if (++s.rearm >= kRearmInterval) {
        s.rearm = 0;
        const u8 carrying = s.carrier ? static_cast<u8>(1u << (s.carrier - 1)) : 0;
        s.dirty |= static_cast<u8>(s.active & ~carrying);
    }
    if (++s.tick < p.rate) return;
    s.tick = 0;
    const u32 next = s.current + 1u;
    s.current = static_cast<u8>(next >= p.length ? 0 : next);
    enter_step(s, p, s.current);
}

inline u32 lowest_bit(u32 mask) {
    u32 i = 0;
    while (!(mask >> i & 1)) ++i;
    return i;
}

inline void park(Tone& tone, u32 root) {
    tone.pitch = static_cast<u8>(root);
    tone.velocity = 1;
    tone.delay = kParkedDelay;
}

// A key's record was replaced or released: whatever it sounded has stopped.
inline void forget(State& s, u32 slot) {
    s.dirty &= static_cast<u8>(~(1u << slot));
    if (s.carrier == slot + 1) {
        s.carrier = 0;
        s.chord_count = 0;
    }
}

bool press(State& s, const Controls&, u32 note, u32 velocity, Plan& plan) {
    plan.count = 0;
    if (note > 127) return false;
    s.event_pending = 1;
    const bool first = !s.active;
    u32 slot = find_slot(s, note);
    if (slot == kSlots) {
        if (s.active == 0xff) return true;  // the SDK cannot hold a ninth root
        slot = lowest_bit(static_cast<u8>(~s.active));
    }
    Key& key = s.keys[slot];
    key.root = static_cast<u8>(note);
    key.velocity = static_cast<u8>(velocity < 1 ? 1 : velocity > 127 ? 127 : velocity);
    key.order = ++s.order_counter;
    s.active |= static_cast<u8>(1u << slot);
    s.pool_dirty = 1;
    forget(s, slot);  // the plan replaces anything this root held
    plan.count = 1;
    park(plan.tones[0], note);
    if (first) {
        s.started = s.carrier = s.chord_count = s.dirty = s.has_note = s.waiting = 0;
        s.gate_left = s.rearm = s.start_wait = 0;
    }
    return true;
}

bool release(State& s, u32 note) {
    s.event_pending = 1;
    const u32 slot = find_slot(s, note);
    if (slot == kSlots) return true;  // a key press() could not hold
    s.active &= static_cast<u8>(~(1u << slot));
    s.pool_dirty = 1;
    forget(s, slot);  // stock silences what it carried
    if (!s.active) {
        s.started = s.waiting = 0;
        s.gate_left = 0;
    }
    return false;  // stock releases this root's notes
}

bool update(State& s, const Controls& c, Update& change) {
    if (!s.draining) {
        s.draining = 1;
        s.drain_calls = 0;
        if (s.event_pending) s.event_pending = 0;
        else clock_step(s, c);
    }
    s.dirty &= s.active;
    if (!s.dirty) {
        s.draining = 0;
        return false;
    }
    const u32 slot = lowest_bit(s.dirty);
    s.dirty &= static_cast<u8>(~(1u << slot));
    change.mode = opxy::player::Replace;
    change.root = s.keys[slot].root;
    change.plan.count = 1;
    if (s.carrier == slot + 1 && s.chord_count) {
        change.plan.count = s.chord_count;  // stock sounds a record's notes in this order
        for (u32 i = 0; i < s.chord_count; ++i) {
            change.plan.tones[i].pitch = s.chord_pitch[i];
            change.plan.tones[i].velocity = s.chord_velocity[i];
            change.plan.tones[i].delay = 0;
        }
    } else if (s.carrier == slot + 1) {
        change.plan.tones[0].pitch = s.pitch;
        change.plan.tones[0].velocity = s.velocity;
        change.plan.tones[0].delay = 0;
    } else {
        park(change.plan.tones[0], change.root);
    }
    if (++s.drain_calls >= kDrainLimit) s.draining = 0;  // the SDK stops calling here
    return true;
}

// Stock reset and all-notes-off: stock has already silenced every record.
void reset(State& s) {
    s.active = s.dirty = s.started = s.carrier = s.chord_count = s.waiting = 0;
    s.gate_left = 0;
    s.draining = s.event_pending = 0;
    s.pool_dirty = 1;
}

// ---- Screen ----

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

// "3.42": BANK then SEED.
inline void append_pattern(char* out, u32 size, u32& at, const Params& p) {
    append_number(out, size, at, p.bank);
    append(out, size, at, ".");
    append_number(out, size, at, p.seed);
}

inline void show(char (&out)[8], u32 value, const char* suffix) {
    u32 at = 0;
    append_number(out, sizeof out, at, value);
    append(out, sizeof out, at, suffix);
}

void display(const State& s, const Controls& c, Display& d) {
    // Lower case: a header value has under 60 px.
    static const char styles[kPatterns][6] = {"off", "up", "down", "up/dn", "dn/up", "u/d+", "d/u+", "order", "conv",
                                              "divrg", "pinky", "thmb", "stu+", "stu-", "std-", "std+", "shuf", "rand",
                                              "drunk", "chord", "chd/1"};
    static const char rates[kRates][6] = {"1/2t", "1/4", "1/4t", "1/8", "1/8t", "1/16", "1/16t", "1/32", "1/32t"};
    static const char notes[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const Params p = read_params(c);
    show(d.values[0], p.seed, "");
    show(d.values[1], p.bank, "");
    show(d.values[2], p.density, p.skip ? "%*" : "%");
    u32 at = 0;
    append(d.values[3], sizeof d.values[3], at, styles[p.style]);
    show(d.values[4], p.length, "");
    at = 0;
    append(d.values[5], sizeof d.values[5], at, rates[opxy::player::choice(c.value[5], kRates)]);
    show(d.values[6], p.gate, "%");
    show(d.values[7], p.range, "");

    // Dots 0..15: the half of the pattern holding the playhead (steps 1..16 when
    // stopped), lit where a step plays, straight from the controls; the playhead
    // flips its cell. Dots 16..23 mark the loop's end: dots 16..19 each draw an end
    // bracket after every fourth cell, and dots 20..23 cover all but one group of
    // four, leaving the one bracket after the loop's last step.
    const bool running = s.active && s.started && !s.waiting;
    const u32 half = running && s.current >= 16 ? 16 : 0;
    for (u32 i = 0; i < 16; ++i)
        if (half + i < p.length && plays(p, half + i)) d.lit |= 1u << i;
    if (running) d.lit ^= 1u << (s.current - half);
    const u32 last = p.length - 1;
    if (last >= half && last < half + 16) {
        const u32 end = last - half;
        d.lit |= 1u << (16 + (end & 3));
        for (u32 group = 0; group < 4; ++group)
            if (group != end >> 2) d.lit |= 1u << (20 + group);
    }
    at = 0;
    if (!running) {
        append(d.text, sizeof d.text, at, "seed ");
        append_pattern(d.text, sizeof d.text, at, p);
        return;
    }
    if (s.has_note) {
        if (s.chord_count) {
            append(d.text, sizeof d.text, at, "chord");
        } else {
            const u32 octave = divide(s.pitch, 12);
            append(d.text, sizeof d.text, at, notes[s.pitch - 12 * octave]);
            if (octave) append_number(d.text, sizeof d.text, at, octave - 1);
            else append(d.text, sizeof d.text, at, "-1");
        }
        append(d.text, sizeof d.text, at, "  ");
    }
    append(d.text, sizeof d.text, at, "#");
    append_pattern(d.text, sizeof d.text, at, p);
}
}  // namespace seed_arp
