#include "aica_tone.hpp"

#include <algorithm>
#include <cstdio>

int main() {
    aica_tone::Samples samples{};
    aica_tone::generate(samples);
    const auto [lowest, highest] = std::minmax_element(samples.begin(), samples.end());
    if(samples.front() != 0 || samples.back() != 0 ||
       *lowest < -12000 || *highest > 12000 ||
       *lowest > -11000 || *highest < 11000 ||
       samples[1] == 0 || samples[32] == 0) {
        std::fputs("FAIL: tone bounds, fades or non-silent PCM\n", stderr);
        return 1;
    }
    const auto original = samples;
    aica_tone::generate(samples);
    if(samples != original) {
        std::fputs("FAIL: generated PCM is not deterministic\n", stderr);
        return 1;
    }
    std::puts("AICA PCM tone: host tests passed");
    return 0;
}
