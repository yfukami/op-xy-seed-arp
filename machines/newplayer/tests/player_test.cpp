#include <cstring>
#include "harness.hpp"

typedef std::vector<u32> Notes;

static void shifts_notes() {
    Host h;
    h.set(0, 12 + 7, 25);  // +7
    h.press(60, 90);
    h.step();
    require(h.pitches() == Notes({67}) && h.events[0].velocity == 90 && h.events[0].time == 0);
    h.release(60);
    require(h.sounding() == 0);

    // Out of range: the note passes through to the instrument unchanged.
    h.press(124);
    require(h.passed_through == 1);
}

static void cleans_up() {
    Host h;
    h.set(0, 12, 25);
    for (u32 i = 0; i < 4; ++i) h.press(60 + i);
    h.step();
    require(h.sounding() == 4);
    h.all_notes_off();
    require(h.sounding() == 0 && h.state.held == 0 && h.stale_updates == 0);
}

static void screen() {
    Host h;
    h.set(0, 0, 25);
    player::Display d = {};
    machine::display(h.state, h.controls, d);
    require(std::strcmp(d.values[0], "-12") == 0 && std::strcmp(d.text, "ready") == 0 && d.lit == 0);
    h.set(0, 12, 25);
    h.press(60); h.press(64);
    player::Display on = {};
    machine::display(h.state, h.controls, on);
    require(std::strcmp(on.values[0], "0") == 0 && std::strcmp(on.text, "playing") == 0 && on.lit == 3);
    for (u32 i = 0; i < 8; ++i) require(std::memchr(on.values[i], 0, sizeof on.values[i]) != nullptr);
}

int main() {
    static_assert(sizeof(machine::State) <= player::state_bytes, "State fits the SDK's area");
    shifts_notes();
    cleans_up();
    screen();
    std::puts("player: all checks passed");
    return 0;
}
