#pragma once

#include <cstdint>

namespace fixed_step {

constexpr std::uint64_t step_us = 16'667; // Approximately 60 updates per second.
constexpr std::uint32_t max_updates_per_frame = 4;

struct TickResult {
    std::uint64_t elapsed_us{};
    std::uint64_t remainder_us{};
    std::uint32_t updates{};
    std::uint64_t dropped_updates{};
    bool clock_rebased{};
};

/// Converts absolute microsecond timestamps into bounded fixed simulation steps.
class Accumulator {
public:
    [[nodiscard]] TickResult tick(std::uint64_t now_us) noexcept {
        if(!initialized_) {
            initialized_ = true;
            previous_us_ = now_us;
            return {};
        }

        if(now_us < previous_us_) {
            previous_us_ = now_us;
            accumulator_us_ = 0;
            return {.clock_rebased = true};
        }

        const std::uint64_t elapsed_us = now_us - previous_us_;
        previous_us_ = now_us;

        // Split the elapsed interval before combining it with the remainder.
        // This preserves an extreme valid uint64_t timestamp jump without
        // overflowing an intermediate microsecond sum.
        const std::uint64_t elapsed_steps = elapsed_us / step_us;
        const std::uint64_t combined_remainder =
            accumulator_us_ + elapsed_us % step_us;
        const std::uint64_t available =
            elapsed_steps + combined_remainder / step_us;
        const auto updates = static_cast<std::uint32_t>(
            available < max_updates_per_frame ? available : max_updates_per_frame);
        const std::uint64_t dropped = available - updates;

        // Keep only the fractional remainder. Whole steps beyond the catch-up
        // limit are deliberately discarded instead of creating a spiral.
        accumulator_us_ = combined_remainder % step_us;
        total_updates_ += updates;
        total_dropped_updates_ += dropped;

        return {elapsed_us, accumulator_us_, updates, dropped, false};
    }

    [[nodiscard]] std::uint64_t total_updates() const noexcept {
        return total_updates_;
    }

    [[nodiscard]] std::uint64_t total_dropped_updates() const noexcept {
        return total_dropped_updates_;
    }

private:
    std::uint64_t previous_us_{};
    std::uint64_t accumulator_us_{};
    std::uint64_t total_updates_{};
    std::uint64_t total_dropped_updates_{};
    bool initialized_{};
};

} // namespace fixed_step
