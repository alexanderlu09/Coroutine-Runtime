#include "doctest/doctest.h"

#include "coro/scheduler.hpp"

#include <unordered_map>
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

class FakeIoDriver : public IoDriver {
public:
    explicit FakeIoDriver(Scheduler& s) : sched_(s) {}

    void wait_readable(int fd, Task* t) override { registered_[fd] = t; }

    void forget(int fd) override { registered_.erase(fd); }

    // No kernel to block in, so "nothing pending" stands in for "nothing will
    // ever happen again" and ends the loop.
    void poll(Nanos) override {
        if (ready_fds_.empty()) {
            sched_.stop();
            return;
        }
        std::vector<int> firing;
        firing.swap(ready_fds_);
        for (int fd : firing) {
            auto it = registered_.find(fd);
            if (it == registered_.end()) continue;  // forgotten before it fired
            Task* t = it->second;
            registered_.erase(it);                  // readiness is one-shot
            sched_.spawn(t);
        }
    }

    void make_ready(int fd) { ready_fds_.push_back(fd); }  // test-only

private:
    Scheduler& sched_;
    std::unordered_map<int, Task*> registered_;
    std::vector<int> ready_fds_;
};


// A connection handler in the shape the echo server will use: it is woken by
// the driver, does its work, and re-arms itself for the next event.
struct Conn {
    FakeIoDriver* io;
    int fd;
    int max_reads;
    int reads = 0;
    Task task{};
};
void conn_read(void* p) {
    auto* c = static_cast<Conn*>(p);
    c->reads++;
    if (c->reads < c->max_reads) {
        c->io->wait_readable(c->fd, &c->task);
        c->io->make_ready(c->fd);   // pretend the peer sent more
    }
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


// ---------------------------------------------------------------------------
// Event loop: Scheduler driven by an IoDriver.
// ---------------------------------------------------------------------------

TEST_CASE("a ready fd runs the task registered for it") {
    Scheduler s;
    FakeIoDriver io{s};
    s.set_io_driver(&io);

    Counter c;
    arm(c.task, bump, &c);

    io.wait_readable(7, &c.task);
    CHECK(c.runs == 0);        // registering does not run anything

    io.make_ready(7);
    s.run();
    CHECK(c.runs == 1);
}

TEST_CASE("a registered fd that never becomes ready never runs") {
    Scheduler s;
    FakeIoDriver io{s};
    s.set_io_driver(&io);

    Counter c;
    arm(c.task, bump, &c);
    io.wait_readable(7, &c.task);

    s.run();                   // no fd made ready
    CHECK(c.runs == 0);
}

TEST_CASE("forget cancels a registration") {
    Scheduler s;
    FakeIoDriver io{s};
    s.set_io_driver(&io);

    Counter c;
    arm(c.task, bump, &c);

    io.wait_readable(7, &c.task);
    io.forget(7);
    io.make_ready(7);          // the kernel says ready, but we cancelled

    s.run();
    CHECK(c.runs == 0);
}

TEST_CASE("only the fd that became ready fires") {
    Scheduler s;
    FakeIoDriver io{s};
    s.set_io_driver(&io);

    Counter a, b;
    arm(a.task, bump, &a);
    arm(b.task, bump, &b);

    io.wait_readable(7, &a.task);
    io.wait_readable(8, &b.task);
    io.make_ready(8);

    s.run();
    CHECK(a.runs == 0);
    CHECK(b.runs == 1);
}

TEST_CASE("readiness is one-shot: a task does not re-fire on its own") {
    Scheduler s;
    FakeIoDriver io{s};
    s.set_io_driver(&io);

    Conn c{&io, 7, 1};         // does not re-arm
    arm(c.task, conn_read, &c);

    io.wait_readable(7, &c.task);
    io.make_ready(7);

    s.run();
    CHECK(c.reads == 1);
}

TEST_CASE("a handler can re-arm itself and be woken again") {
    Scheduler s;
    FakeIoDriver io{s};
    s.set_io_driver(&io);

    Conn c{&io, 7, 4};
    arm(c.task, conn_read, &c);

    io.wait_readable(7, &c.task);
    io.make_ready(7);

    s.run();
    CHECK(c.reads == 4);       // woken once per simulated arrival
}

TEST_CASE("queued tasks and io wakeups coexist in one loop") {
    Scheduler s;
    FakeIoDriver io{s};
    s.set_io_driver(&io);

    Counter queued, from_io;
    arm(queued.task, bump, &queued);
    arm(from_io.task, bump, &from_io);

    s.spawn(&queued.task);            // already runnable
    io.wait_readable(7, &from_io.task);
    io.make_ready(7);

    s.run();
    CHECK(queued.runs == 1);
    CHECK(from_io.runs == 1);
}
