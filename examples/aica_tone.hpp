#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace aica_tone {

constexpr std::uint32_t sample_rate = 11025;
constexpr std::size_t sample_count = 4096;
constexpr std::size_t fade_samples = 128;
static_assert(sample_count < 65535, "KOS raw SFX sample limit exceeded");
static_assert(sample_count * sizeof(std::int16_t) % 32 == 0,
              "KOS raw SFX byte length must be a multiple of 32");

using Samples = std::array<std::int16_t, sample_count>;

// One short mono PCM tone; soften both ends to avoid abrupt clicks.
inline void generate(Samples &samples) noexcept {
    constexpr float frequency = 440.0f;
    constexpr float tau = 6.28318530718f;
    for(std::size_t i = 0; i < samples.size(); ++i) {
        const float attack = std::min(1.0f, static_cast<float>(i) / fade_samples);
        const float release = std::min(
            1.0f, static_cast<float>(samples.size() - 1 - i) / fade_samples);
        const float phase = tau * frequency * static_cast<float>(i) / sample_rate;
        samples[i] = static_cast<std::int16_t>(12000.0f *
                                                std::min(attack, release) *
                                                std::sin(phase));
    }
}

} // namespace aica_tone
