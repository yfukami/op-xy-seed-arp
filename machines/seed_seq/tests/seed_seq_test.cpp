#include <algorithm>
#include <cstring>
#include "harness.hpp"

typedef std::vector<u32> Notes;
using namespace machine;
enum { R2T, R4, R4T, R8, R8T, R16, R16T, R32, R32T };
static const u32 kRateSteps[9] = {320, 240, 160, 120, 80, 60, 40, 30, 20};

struct Settings {
    u32 seed = 42, bank = 0, density = 60, style = Up, length = 16, rate = R16, gate = 50, range = 1;
    bool skip = false;  // DENSITY's starred half
};

static void configure(Host& h, const Settings& v) {
    h.set(0, v.seed, 255); h.set(1, v.bank, 255); h.set(2, v.skip ? 40 - v.density / 5 : v.density / 5, kDensities); h.set(3, v.style - 1, kStyles);
    h.set(4, v.length - 1, 32); h.set(5, v.rate, 9); h.set(6, (v.gate - 10) / 5, kGates); h.set(7, v.range - 1, 3);
}

struct Span { u32 pitch, on, off; };
static std::vector<Span> spans(const Host& h) {
    std::vector<Span> out;
    for (const Event& e : h.events) {
        if (e.on) { out.push_back(Span{e.pitch, e.time, 0}); continue; }
        for (size_t i = out.size(); i-- > 0;)
            if (out[i].pitch == e.pitch && !out[i].off) { out[i].off = e.time; break; }
    }
    return out;
}

// Plays `loops` loops of the pattern on `keys`, pressed together; returns its notes.
static std::vector<Span> play(const Settings& v, const Notes& keys, u32 loops) {
    Host h;
    configure(h, v);
    for (u32 k : keys) h.press(k);
    h.step(v.length * kRateSteps[v.rate] * loops);
    return spans(h);
}

// The notes of one loop, with times relative to the loop's start.
static std::vector<Span> loop(const std::vector<Span>& all, const Settings& v, u32 index) {
    const u32 length = v.length * kRateSteps[v.rate], begin = index * length;
    std::vector<Span> out;
    for (Span s : all)
        if (s.on >= begin && s.on < begin + length) { s.on -= begin; s.off = s.off ? s.off - begin : 0; out.push_back(s); }
    return out;
}

static bool same(const std::vector<Span>& a, const std::vector<Span>& b, bool pitches = true) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].on != b[i].on || (pitches && a[i].pitch != b[i].pitch)) return false;
    return true;
}

static Notes pitches_of(const std::vector<Span>& notes) {
    Notes out;
    for (const Span& s : notes) out.push_back(s.pitch);
    return out;
}

static std::vector<u32> step_numbers(const std::vector<Span>& notes, u32 rate) {
    std::vector<u32> out;
    for (const Span& s : notes) out.push_back(s.on / rate);
    return out;
}

static const Notes kChord = {60, 64, 67};

static void determinism() {
    // Every style plays the same loop each time, and the same in a fresh OP-XY.
    for (u32 style = Up; style < kPatterns; ++style) {
        Settings v;
        v.style = style;
        const std::vector<Span> all = play(v, kChord, 3);
        require(all.size() > 6);
        require(same(loop(all, v, 0), loop(all, v, 1)) && same(loop(all, v, 1), loop(all, v, 2)));
        require(same(all, play(v, kChord, 3)));
    }
    // Different seeds and banks give different rhythms.
    Settings v;
    u32 different = 0;
    for (u32 seed = 0; seed < 20; ++seed) {
        Settings other = v;
        other.seed = seed;
        if (!same(play(other, kChord, 1), play(v, kChord, 1), false)) ++different;
        other.seed = v.seed;
        other.bank = 1 + seed;
        if (!same(play(other, kChord, 1), play(v, kChord, 1), false)) ++different;
    }
    require(different >= 38);
    Settings last;
    last.bank = 254; last.seed = 254;
    require(!play(last, kChord, 1).empty());
}

static void density() {
    Settings none;
    none.density = 0;
    require(play(none, kChord, 2).empty());
    Settings full;
    full.density = 100;
    require(play(full, kChord, 1).size() == 16);
    // Every density level adds steps to the one below it, never moving any, for 100 seeds.
    Host h;
    for (u32 seed = 0; seed < 100; ++seed) {
        u32 below = 0;
        for (u32 level = 0; level <= 20; ++level) {
            Settings v;
            v.seed = seed; v.density = 5 * level;
            configure(h, v);
            player::Display d = {};
            display(h.state, h.controls, d);
            require((d.lit & below) == below);
            if (level) require(d.lit & 1);  // step 1 anchors the downbeat
            below = d.lit;
        }
        require(below == 0xffffu);
    }
}

static void styles() {
    // Each played step takes the pattern's next note; rests do not skip any, and
    // every loop starts the pattern again.
    Settings v;
    v.density = 40;
    const std::vector<Span> up = play(v, kChord, 2);
    const std::vector<Span> first = loop(up, v, 0), second = loop(up, v, 1);
    require(first.size() >= 3);
    for (size_t i = 0; i < first.size(); ++i) require(first[i].pitch == kChord[i % 3]);
    require(pitches_of(first) == pitches_of(second));
    // The rhythm comes from the seed alone: every style plays on the same steps.
    for (u32 style = Up; style < kPatterns; ++style) {
        Settings s = v;
        s.style = style;
        Notes steps;
        for (u32 n : step_numbers(play(s, kChord, 1), 60)) if (steps.empty() || steps.back() != n) steps.push_back(n);
        require(steps == step_numbers(first, 60));
    }
    Settings down = v;
    down.style = Down;
    const std::vector<Span> d = play(down, kChord, 1);
    for (size_t i = 0; i < d.size(); ++i) require(d[i].pitch == kChord[2 - i % 3]);
    Settings order = v;
    order.style = Order;
    const std::vector<Span> o = play(order, {67, 60, 64}, 1);
    for (size_t i = 0; i < o.size(); ++i) require(o[i].pitch == Notes({67, 60, 64})[i % 3]);
    // RANGE widens the notes the style walks through.
    Settings wide = v;
    wide.density = 100; wide.range = 2;
    require(pitches_of(play(wide, kChord, 1)) == Notes({60, 64, 67, 72, 76, 79, 60, 64, 67, 72, 76, 79, 60, 64, 67, 72}));
}

static void skipping() {
    // DENSITY starred: the same rhythm, but each played step plays the note it
    // would at 100%, for every style, randomness included.
    for (u32 style = Up; style < kPatterns; ++style) {
        Settings all, starred, plain;
        all.style = starred.style = plain.style = style;
        all.density = 100; all.range = starred.range = plain.range = 2;
        starred.density = plain.density = 45; starred.skip = true;
        const std::vector<Span> every = play(all, kChord, 1), skipped = play(starred, kChord, 1);
        // The rhythm is the same either way (a step may hold a chord in one and a note in the other).
        std::vector<u32> a, b;
        for (const Span& s : skipped) if (a.empty() || a.back() != s.on) a.push_back(s.on);
        for (const Span& s : play(plain, kChord, 1)) if (b.empty() || b.back() != s.on) b.push_back(s.on);
        require(a == b);
        for (const Span& s : skipped) {
            Notes at;
            for (const Span& e : every) if (e.on == s.on) at.push_back(e.pitch);
            require(std::find(at.begin(), at.end(), s.pitch) != at.end());
        }
    }
    // Unstarred, rests skip nothing: UP plays C E G in turn whatever the rhythm.
    Settings plain;
    plain.density = 45;
    const std::vector<Span> notes = play(plain, kChord, 1);
    for (size_t i = 0; i < notes.size(); ++i) require(notes[i].pitch == kChord[i % 3]);
    // The encoder: 0..100%, then 95%*..0%*.
    Host h;
    const u32 positions[5][3] = {{0, 0, 0}, {20, 100, 0}, {21, 95, 1}, {28, 60, 1}, {40, 0, 1}};
    for (const auto& a : positions) {
        h.set(2, a[0], kDensities);
        const Params p = read_params(h.controls);
        require(p.density == a[1] && p.skip == (a[2] != 0));
    }
    Settings none;
    none.density = 0; none.skip = true;
    require(play(none, kChord, 2).empty());
}

static void chords() {
    // CHORD: every held key each played step, climbing RANGE one octave per hit,
    // the lowest note sent last so a monophonic instrument keeps it.
    Settings v;
    v.density = 100; v.style = Chord; v.range = 2;
    Host h;
    configure(h, v);
    h.press(67); h.press(64); h.press(60);  // a sequenced chord arrives highest first
    h.step(60 * 4);
    std::vector<Event> on;
    for (const Event& e : h.events) if (e.on) on.push_back(e);
    require(on.size() == 12);
    const u32 lowest[4] = {60, 72, 60, 72};
    for (u32 hit = 0; hit < 4; ++hit) {
        require(on[3 * hit + 2].pitch == lowest[hit]);  // last of each hit: its lowest note
        for (u32 i = 0; i < 3; ++i) require(on[3 * hit + i].time == 60 * hit);
    }
    require(h.sounding() <= 3 && h.largest_drain <= 8 && h.stale_updates == 0);
    // CHD/1: a chord, then UP.
    Settings c = v;
    c.style = ChordThenUp; c.range = 1;
    const Notes expected = {67, 64, 60, 60, 64, 67};
    require(pitches_of(play(c, kChord, 1)).size() >= 6);
    Notes start = pitches_of(play(c, kChord, 1));
    start.resize(6);
    require(start == expected);
}

static void random_styles_repeat() {
    // SHUF, RAND and DRUNK draw from the seed: the same every loop, after reloading, and per seed.
    for (u32 style : {u32(Shuffle), u32(Random), u32(Drunk)}) {
        Settings v;
        v.style = style; v.density = 100; v.range = 2;
        const std::vector<Span> all = play(v, kChord, 3);
        require(same(loop(all, v, 0), loop(all, v, 2)));
        Settings other = v;
        other.seed = 7;
        require(!same(play(other, kChord, 1), loop(all, v, 0)));
    }
}

static void transposition() {
    // Another chord of three keys: the same rhythm, and the same places in its own notes.
    Settings v;
    v.range = 2; v.style = Pinky;
    const std::vector<Span> c = play(v, {60, 64, 67}, 1), d = play(v, {62, 65, 69}, 1);
    require(same(c, d, false));
    const Notes cs = {60, 64, 67, 72, 76, 79}, ds = {62, 65, 69, 74, 77, 81};
    for (size_t i = 0; i < c.size(); ++i)
        require(std::find(cs.begin(), cs.end(), c[i].pitch) - cs.begin() ==
                std::find(ds.begin(), ds.end(), d[i].pitch) - ds.begin());
}

static void length_rates_and_gate() {
    for (u32 rate = 0; rate < 9; ++rate) {
        Settings v;
        v.rate = rate; v.length = 5; v.density = 100;
        const std::vector<Span> all = play(v, kChord, 3);
        require(all.size() == 15);
        for (u32 i = 0; i < all.size(); ++i) require(all[i].on == i * kRateSteps[rate]);
        require(same(loop(all, v, 0), loop(all, v, 2)));
    }
    for (u32 gate : {10u, 50u, 100u}) {
        Settings v;
        v.density = 100; v.gate = gate;
        for (const Span& s : play(v, kChord, 1))
            if (s.off) require(s.off - s.on == std::max(1u, 60 * gate / 100));
    }
}

// Chords played by hand: a key arriving alone waits about 15 ms (7 steps without
// a tempo) for the rest, so the pattern starts as it does with every key held
// together. Keys arriving together start at once. The steps count from the first key.
static void hand_played_chords() {
    const u32 wait = kDefaultChordWindow;
    Settings v;
    v.density = 100; v.range = 2;
    const std::vector<Span> together = play(v, kChord, 1);
    require(together[0].on == 0);
    const Notes orders[3] = {{67, 64, 60}, {64, 60, 67}, {60, 67, 64}};
    for (const Notes& keys : orders) {
        Host h;
        configure(h, v);
        h.press(keys[0]);
        h.step(2);
        h.press(keys[1]);
        h.step(2);
        h.press(keys[2]);
        h.step(60 * 16 - 4);
        const std::vector<Span> notes = spans(h);
        require(notes.size() == together.size() && notes[0].on == wait);
        for (size_t i = 0; i < notes.size(); ++i) {
            require(notes[i].pitch == together[i].pitch);
            if (i) require(notes[i].on == together[i].on);  // the grid counts from the first key
        }
    }
    Host alone;
    configure(alone, v);
    alone.press(60);
    alone.step(wait);
    require(alone.events.empty());
    alone.step(1);
    require(alone.pitches() == Notes({60}));
    require(chord_window(1200, 60) == 7 && chord_window(600, 60) == 3);
    require(chord_window(3000, 20) == 10 && chord_window(0, 60) == 7);
}

static void cleanup() {
    Settings v;
    v.gate = 100;
    Host h;
    configure(h, v);
    for (u32 k : kChord) h.press(k);
    h.step(200);
    for (u32 k : kChord) h.release(k);
    require(h.sounding() == 0);
    h.step(500);
    require(h.sounding() == 0);
    h.press(62);
    h.step(kDefaultChordWindow + 1);  // a key alone waits about 15 ms
    require(h.state.current == 0 && h.pitches().back() == 62);
    h.all_notes_off();
    require(h.sounding() == 0);

    Settings dense;
    dense.density = 100; dense.style = Chord;
    Host full;
    configure(full, dense);
    for (u32 i = 0; i < 9; ++i) full.press(60 + i);
    require(full.passed_through == 0 && full.held == 8 && !full.find(68));
    full.release(68);
    full.step(600);
    for (u32 i = 0; i < 8; ++i) { full.release(60 + i); full.step(23); }
    require(full.sounding() == 0 && full.stale_updates == 0 && full.largest_drain <= 8);

    // A held key that never carries a note never sounds its parked placeholder:
    // find a seed whose only played step is the downbeat, so one key stays idle.
    Settings sparse;
    sparse.rate = R4; sparse.density = 5;
    Host probe;
    u32 seed = 255;
    for (u32 candidate = 0; candidate < 255 && seed == 255; ++candidate) {
        sparse.seed = candidate;
        configure(probe, sparse);
        player::Display d = {};
        display(probe.state, probe.controls, d);
        if (d.lit == 1) seed = candidate;
    }
    require(seed < 255);
    sparse.seed = seed;
    Host idle;
    configure(idle, sparse);
    idle.press(60);
    idle.press(72);
    idle.step(140000);
    for (const Event& e : idle.events) if (e.on) require(e.velocity != 1 && e.pitch == 60);
    require(idle.stale_updates == 0);
}

static void screen() {
    Settings v;
    v.seed = 7; v.bank = 3; v.density = 45; v.skip = true; v.style = StairDownRising; v.length = 20; v.rate = R16T; v.gate = 35; v.range = 2;
    Host h;
    configure(h, v);
    player::Display d = {};
    display(h.state, h.controls, d);
    const char* const expected[8] = {"7", "3", "45%*", "std+", "20", "1/16t", "35%", "2"};
    for (u32 i = 0; i < 8; ++i) {
        require(std::memchr(d.values[i], 0, sizeof d.values[i]) != nullptr);
        require(std::strcmp(d.values[i], expected[i]) == 0);
    }
    require(std::strcmp(d.text, "seed 3.7") == 0);
    // The lit cells are the rhythm, before anything plays, and follow SEED at once.
    Settings p;
    configure(h, p);
    player::Display shown = {};
    display(h.state, h.controls, shown);
    u32 rhythm = 0;
    for (u32 step : step_numbers(play(p, kChord, 1), 60)) rhythm |= 1u << step;
    require(shown.lit == rhythm && (rhythm & 1));
    p.seed = 8;
    configure(h, p);
    player::Display next = {};
    display(h.state, h.controls, next);
    require(next.lit != shown.lit);

    // Playing: the playhead, and the note with the pattern number.
    Settings q;
    q.density = 100; q.length = 32;
    Host r;
    configure(r, q);
    r.press(61); r.press(64);
    r.step(60 * 21 + 1);  // step 22
    player::Display on = {};
    display(r.state, r.controls, on);
    require(std::strcmp(on.text, "E4  #0.42") == 0);  // step 22 of UP over C#4 E4: the second note
    require((on.lit & 0xffffu) == 0xffffu);
    require(((on.lit >> 16) & 0xfu) == (1u << 1) && ((on.lit >> 20) & 0xfu) == (0xfu & ~2u));
    Settings ch = q;
    ch.style = Chord;
    configure(r, ch);
    r.step(60);
    player::Display chord = {};
    display(r.state, r.controls, chord);
    require(std::strcmp(chord.text, "chord  #0.42") == 0);
}

int main() {
    static_assert(sizeof(State) <= player::state_bytes, "State fits the SDK's area");
    determinism();
    density();
    styles();
    skipping();
    chords();
    random_styles_repeat();
    transposition();
    length_rates_and_gate();
    hand_played_chords();
    cleanup();
    screen();
    std::puts("seed_seq: all checks passed");
    return 0;
}
