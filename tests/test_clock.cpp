#include "doctest/doctest.h"

#include "coro/clock.hpp"

using namespace coro;

TEST_CASE("ManualClock starts where we tell it to") {
    ManualClock c{millis(5)};
    CHECK(c.now() == 5'000'000);
}

TEST_CASE("ManualClock advances by exactly what we ask") {
    ManualClock c;
    CHECK(c.now() == 0);
    c.advance(millis(10));
    CHECK(c.now() == millis(10));
    c.advance(micros(500));
    CHECK(c.now() == millis(10) + micros(500));
}

TEST_CASE("ManualClock never moves backwards") {
    ManualClock c{seconds(1)};
    c.advance_to(millis(1));  // in the past -- must be ignored
    CHECK(c.now() == seconds(1));
    c.advance_to(seconds(2));
    CHECK(c.now() == seconds(2));
}

TEST_CASE("SystemClock is monotonic and actually moves") {
    auto& c = SystemClock::instance();
    const Nanos a = c.now();
    Nanos b = c.now();
    // Spin until the clock ticks; on any real machine this is immediate.
    while (b == a) b = c.now();
    CHECK(b > a);
}

TEST_CASE("a Clock& hides which implementation it is") {
    // This is the property the whole simulator depends on: code that takes a
    // Clock& cannot tell real time from simulated time.
    ManualClock manual{millis(42)};
    Clock& as_base = manual;
    CHECK(as_base.now() == millis(42));
    manual.advance(millis(1));
    CHECK(as_base.now() == millis(43));
}
