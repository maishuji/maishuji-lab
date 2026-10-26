#include "maishuji/pvr.hpp"

#include <kos.h>
#include <sh4zam/shz_version.h>

#include <cstdint>

namespace {

struct VersionFields {
    std::uint8_t major;
    std::uint16_t minor;
    std::uint8_t patch;
};

VersionFields linked_version_fields() noexcept {
    VersionFields fields{};
    shz_version_fields(shz_version_linked(), &fields.major, &fields.minor,
                       &fields.patch);
    return fields;
}

bool versions_match(const VersionFields &linked) noexcept {
    return shz_version_linked() == SHZ_VERSION &&
           linked.major == SHZ_VERSION_MAJOR &&
           linked.minor == SHZ_VERSION_MINOR &&
           linked.patch == SHZ_VERSION_PATCH;
}

maishuji::Status draw_empty_frame(maishuji::Pvr &pvr) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList opaque;
    status = frame.begin_list(opaque, maishuji::List::Opaque);
    if(maishuji::failed(status))
        return status;

    status = opaque.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    const VersionFields linked = linked_version_fields();
    if(!versions_match(linked)) {
        dbglog(DBG_ERROR,
               "maishuji: SH4ZAM compatibility failed "
               "(headers %u.%u.%u; linked %u.%u.%u)\n",
               SHZ_VERSION_MAJOR, SHZ_VERSION_MINOR, SHZ_VERSION_PATCH,
               linked.major, linked.minor, linked.patch);
        return 1;
    }

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    dbglog(DBG_NOTICE,
           "maishuji: SH4ZAM compatibility passed "
           "(headers %u.%u.%u; linked %u.%u.%u)\n",
           SHZ_VERSION_MAJOR, SHZ_VERSION_MINOR, SHZ_VERSION_PATCH,
           linked.major, linked.minor, linked.patch);

    constexpr int validation_frames = 600;
    for(int frame = 0; frame < validation_frames; ++frame) {
        status = draw_empty_frame(pvr);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR, "maishuji: frame %d failed: %s\n", frame,
                   maishuji::status_name(status));
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = pvr.shutdown();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR shutdown failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    return 0;
}
