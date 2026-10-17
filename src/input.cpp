#include "maishuji/input.hpp"

namespace maishuji {

const char *input_availability_name(InputAvailability availability) noexcept {
    switch(availability) {
    case InputAvailability::Disconnected: return "DISCONNECTED";
    case InputAvailability::Unsupported: return "UNSUPPORTED";
    case InputAvailability::Unavailable: return "UNAVAILABLE";
    case InputAvailability::Valid: return "VALID (CACHED)";
    case InputAvailability::InvalidPort: return "INVALID PORT";
    }
    return "UNKNOWN";
}

} // namespace maishuji
