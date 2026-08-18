#pragma once
//
// Time. Read this comment before writing any other file in this project.
//
// RULE: no code in this repository may call std::chrono::steady_clock::now(),
// time(), gettimeofday(), or any other wall-clock source directly. Everything
// that needs to know what time it is takes a `Clock&` and asks it.
//
// This looks like pointless indirection today. It is the single decision that
// makes the deterministic simulator (block B15) possible later: in simulation
// mode, time is a uint64_t we control, so a 30-second timeout test runs in
// microseconds and a test that fails once fails identically forever.
//
// Retrofitting this rule in week 4 means touching every file. Following it
// from day one costs nothing.
//
#include <chrono>
#include <cstdint>

namespace coro {

// Nanoseconds since some arbitrary epoch. Monotonic, never decreasing.
// 64 bits of nanoseconds is ~584 years, so overflow is not a concern.
using Nanos = std::uint64_t;

constexpr Nanos nanos(std::uint64_t n)   noexcept { return n; }
constexpr Nanos micros(std::uint64_t n)  noexcept { return n * 1'000; }
constexpr Nanos millis(std::uint64_t n)  noexcept { return n * 1'000'000; }
constexpr Nanos seconds(std::uint64_t n) noexcept { return n * 1'000'000'000; }

class Clock {
public:
    virtual ~Clock() = default;
    virtual Nanos now() const noexcept = 0;
};

// Real time, backed by steady_clock. Used by every non-simulated backend.
//
// NOTE: `now()` is virtual, so every call is an indirect branch. That is fine
// today (a clock read is nowhere near the hot path yet). If profiling later
// shows it matters, the fix is to make Clock a template parameter instead of
// a base class -- write that up as an ADR when you measure it, not before.
class SystemClock final : public Clock {
public:
    Nanos now() const noexcept override;

    // Convenience: most call sites want one shared instance.
    static SystemClock& instance() noexcept;
};

// Time as a variable. The simulator drives this.
//
// Deliberately NOT thread-safe: the simulation backend is single-threaded by
// design, and that is the whole point of it. If you ever feel the urge to add
// a mutex here, you have misunderstood something -- stop and re-read B15.
class ManualClock final : public Clock {
public:
    explicit ManualClock(Nanos start = 0) noexcept : now_(start) {}

    Nanos now() const noexcept override { return now_; }

    void advance(Nanos delta) noexcept { now_ += delta; }

    // Jump straight to a future instant (used to skip to the next timer
    // expiry when nothing is runnable). Never moves backwards.
    void advance_to(Nanos t) noexcept {
        if (t > now_) now_ = t;
    }

private:
    Nanos now_;
};

}  // namespace coro
