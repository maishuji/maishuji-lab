#include "fixed_step.hpp"

#include <cstdio>

namespace {

bool expect(bool condition, const char *message) {
    if(condition) return true;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

} // namespace

int main() {
    bool passed = true;

    fixed_step::Accumulator clock;
    auto tick = clock.tick(1'000'000);
    passed &= expect(tick.updates == 0 && tick.elapsed_us == 0,
                     "the first timestamp only initializes the clock");

    tick = clock.tick(1'008'000);
    passed &= expect(tick.updates == 0 && tick.remainder_us == 8'000,
                     "a partial interval remains accumulated");

    tick = clock.tick(1'016'667);
    passed &= expect(tick.updates == 1 && tick.remainder_us == 0,
                     "partial intervals combine into one fixed step");

    tick = clock.tick(1'050'001);
    passed &= expect(tick.updates == 2 && tick.remainder_us == 0,
                     "one render interval can produce multiple updates");

    tick = clock.tick(1'050'001);
    passed &= expect(tick.updates == 0 && tick.elapsed_us == 0,
                     "an identical timestamp produces no update");

    tick = clock.tick(900'000);
    passed &= expect(tick.clock_rebased && tick.updates == 0 && tick.remainder_us == 0,
                     "a backward timestamp rebases without unsigned underflow");

    tick = clock.tick(900'000 + fixed_step::step_us * 10 + 123);
    passed &= expect(tick.updates == fixed_step::max_updates_per_frame &&
                         tick.dropped_updates == 6 && tick.remainder_us == 123,
                     "large stalls bound catch-up and count discarded steps");
    passed &= expect(clock.total_updates() == 7 &&
                         clock.total_dropped_updates() == 6,
                     "lifetime counters include completed and dropped updates");

    fixed_step::Accumulator rounding;
    (void)rounding.tick(0);
    for(std::uint64_t timestamp = 1'000; timestamp <= 50'000; timestamp += 1'000) {
        (void)rounding.tick(timestamp);
    }
    passed &= expect(rounding.total_updates() == 2,
                     "small elapsed intervals retain their integer remainder");
    const auto final_tick = rounding.tick(3 * fixed_step::step_us);
    passed &= expect(final_tick.updates == 1 && final_tick.remainder_us == 0,
                     "accumulated rounding eventually completes the expected step");

    if(!passed) return 1;
    std::puts("Fixed-step accumulator: host tests passed");
    return 0;
}
