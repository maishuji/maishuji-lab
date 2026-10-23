#include "aica_tone.hpp"

#include <dc/sound/sfxmgr.h>
#include <dc/sound/sound.h>
#include <kos.h>

namespace {

// A short, owned sample. KOS copies this into AICA sound RAM when loaded.
alignas(32) aica_tone::Samples samples{};

} // namespace

int main() {
    if(snd_init() < 0) {
        dbglog(DBG_ERROR, "maishuji: AICA sound initialization failed\n");
        return 1;
    }

    aica_tone::generate(samples);
    const sfxhnd_t effect = snd_sfx_load_raw_buf(
        reinterpret_cast<char *>(samples.data()),
        samples.size() * sizeof(samples[0]), aica_tone::sample_rate, 16, 1);
    if(effect == SFXHND_INVALID) {
        dbglog(DBG_ERROR, "maishuji: AICA PCM load failed\n");
        return 1;
    }

    // A 4096-sample sound at 11025 Hz lasts about 372 ms. Wait 500 ms
    // after every play, including a failed later play, before freeing AICA RAM.
    constexpr int pans[] = {0, 128, 255};
    bool played_all = true;
    for(const int pan : pans) {
        if(snd_sfx_play(effect, 180, pan) < 0) {
            dbglog(DBG_ERROR, "maishuji: AICA play failed (pan=%d)\n", pan);
            played_all = false;
            break;
        }
        thd_sleep(500);
    }
    if(!played_all) thd_sleep(500);
    snd_sfx_unload(effect);

    if(!played_all) return 1;
    dbglog(DBG_NOTICE,
           "maishuji: AICA sound playback requests passed (left/center/right; audibility unverified)\n");
    return 0;
}
