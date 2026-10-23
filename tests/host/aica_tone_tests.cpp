#include "aica_tone.hpp"

#include <algorithm>
#include <cstddef>
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
    std::size_t sign_changes = 0;
    for(std::size_t i = 2; i < samples.size(); ++i) {
        if((samples[i - 1] < 0 && samples[i] >= 0) ||
           (samples[i - 1] > 0 && samples[i] <= 0)) {
            ++sign_changes;
        }
    }
    if(sign_changes < 320 || sign_changes > 330) {
        std::fprintf(stderr, "FAIL: expected a 440 Hz-scale tone, got %zu sign changes\n",
                     sign_changes);
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
