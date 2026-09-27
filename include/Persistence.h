#pragma once

namespace Persistence {
constexpr bool enabled() {
#ifdef VOXELFRONTIER_STANDALONE_DEMO
    return false;
#else
    return true;
#endif
}
} // namespace Persistence
