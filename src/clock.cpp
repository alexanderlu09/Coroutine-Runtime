#include "coro/clock.hpp"

namespace coro {

Nanos SystemClock::now() const noexcept {
    const auto d = std::chrono::steady_clock::now().time_since_epoch();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(d);
    return static_cast<Nanos>(ns.count());
}

SystemClock& SystemClock::instance() noexcept {
    static SystemClock c;
    return c;
}

}  // namespace coro
