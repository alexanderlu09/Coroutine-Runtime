#include "doctest/doctest.h"

#include "coro/scheduler.hpp"

#include <vector>

using namespace coro;

namespace {

// Each of these is a hand-rolled "closure": state + a function that knows how
// to cast void* back to it.

struct Counter {
    int runs = 0;
    Task task{};
};
void bump(void* p) { static_cast<Counter*>(p)->runs++; }

struct Recorder {
    std::vector<int>* log;
    int id;
    Task task{};
};
void record(void* p) {
    auto* r = static_cast<Recorder*>(p);
    r->log->push_back(r->id);
}

struct Spawner {
    Scheduler* sched;
    Task* child;
    Task task{};
};
void spawn_child(void* p) {
    auto* s = static_cast<Spawner*>(p);
    s->sched->spawn(s->child);
}

// Re-queues itself until it has run `remaining` times. This is the shape every
// async handler will have.
struct Looper {
    Scheduler* sched;
    int remaining;
    int runs = 0;
    Task task{};
};
void loop_fn(void* p) {
    auto* l = static_cast<Looper*>(p);
    l->runs++;
    if (--l->remaining > 0) l->sched->spawn(&l->task);
}

void arm(Task& t, void (*fn)(void*), void* state) {
    t.fn = fn;
    t.state = state;
}

}  // namespace

TEST_CASE("running an empty scheduler does nothing") {
    Scheduler s;
    s.run();  // must return, not hang
}

TEST_CASE("a spawned task runs exactly once") {
    Scheduler s;
    Counter c;
    arm(c.task, bump, &c);

    s.spawn(&c.task);
    CHECK(c.runs == 0);  // spawn queues, it does not call

    s.run();
    CHECK(c.runs == 1);

    s.run();  // queue is drained; nothing left to do
    CHECK(c.runs == 1);
}

TEST_CASE("tasks run in the order they were spawned") {
    Scheduler s;
    std::vector<int> log;
    Recorder r0{&log, 0}, r1{&log, 1}, r2{&log, 2};
    arm(r0.task, record, &r0);
    arm(r1.task, record, &r1);
    arm(r2.task, record, &r2);

    s.spawn(&r0.task);
    s.spawn(&r1.task);
    s.spawn(&r2.task);
    s.run();

    REQUIRE(log.size() == 3);
    CHECK(log[0] == 0);
    CHECK(log[1] == 1);
    CHECK(log[2] == 2);
}

TEST_CASE("one scheduler holds tasks of unrelated types") {
    Scheduler s;
    Counter c;
    std::vector<int> log;
    Recorder r{&log, 7};
    arm(c.task, bump, &c);
    arm(r.task, record, &r);

    s.spawn(&c.task);
    s.spawn(&r.task);
    s.run();

    CHECK(c.runs == 1);
    REQUIRE(log.size() == 1);
    CHECK(log[0] == 7);
}

TEST_CASE("a task spawned during run is picked up by that same run") {
    Scheduler s;
    Counter c;
    arm(c.task, bump, &c);

    Spawner sp{&s, &c.task};
    arm(sp.task, spawn_child, &sp);

    s.spawn(&sp.task);
    s.run();

    CHECK(c.runs == 1);
}

TEST_CASE("a task can re-queue itself and stop on its own terms") {
    Scheduler s;
    Looper l{&s, 5};
    arm(l.task, loop_fn, &l);

    s.spawn(&l.task);
    s.run();

    CHECK(l.runs == 5);
    CHECK(l.remaining == 0);
}

TEST_CASE("interleaved spawns and re-queues all complete") {
    Scheduler s;
    Looper a{&s, 3};
    Looper b{&s, 2};
    Counter c;
    arm(a.task, loop_fn, &a);
    arm(b.task, loop_fn, &b);
    arm(c.task, bump, &c);

    s.spawn(&a.task);
    s.spawn(&c.task);
    s.spawn(&b.task);
    s.run();

    CHECK(a.runs == 3);
    CHECK(b.runs == 2);
    CHECK(c.runs == 1);
}
