#include "doctest/doctest.h"

#include "coro/kqueue_driver.hpp"
#include "coro/scheduler.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace coro;

namespace {

// A connected pair of fds with no network involved: writing to one end makes
// the other end readable, which is all the driver needs to be exercised.
struct SocketPair {
    int a = -1;
    int b = -1;

    SocketPair() {
        int fds[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) std::abort();
        a = fds[0];
        b = fds[1];
        set_nonblocking(a);
        set_nonblocking(b);
    }
    ~SocketPair() {
        if (a >= 0) ::close(a);
        if (b >= 0) ::close(b);
    }
    SocketPair(const SocketPair&) = delete;
    SocketPair& operator=(const SocketPair&) = delete;

    static void set_nonblocking(int fd) {
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) std::abort();
    }
};

void send_byte(int fd) {
    const char c = 'x';
    if (::write(fd, &c, 1) != 1) std::abort();
}

void arm(Task& t, void (*fn)(void*), void* state) {
    t.fn = fn;
    t.state = state;
}

// Reads once and stops the loop.
struct OneShot {
    Scheduler* sched;
    int fd;
    int reads = 0;
    ssize_t bytes = -1;
    Task task{};
};
void one_shot_read(void* p) {
    auto* s = static_cast<OneShot*>(p);
    char buf[64];
    s->bytes = ::read(s->fd, buf, sizeof(buf));
    s->reads++;
    s->sched->stop();
}

// Reads, re-arms, and pokes the peer so another event arrives -- the shape a
// real connection handler has.
struct Repeater {
    Scheduler* sched;
    KqueueDriver* io;
    int fd;
    int peer;
    int target;
    int reads = 0;
    Task task{};
};
void repeat_read(void* p) {
    auto* r = static_cast<Repeater*>(p);
    char buf[64];
    (void)::read(r->fd, buf, sizeof(buf));
    r->reads++;
    if (r->reads < r->target) {
        r->io->wait_readable(r->fd, &r->task);
        send_byte(r->peer);
    } else {
        r->sched->stop();
    }
}

// Records that it ran; never stops the loop.
struct Flag {
    bool fired = false;
    Task task{};
};
void set_flag(void* p) { static_cast<Flag*>(p)->fired = true; }

}  // namespace

TEST_CASE("kqueue wakes a task when a socket becomes readable") {
    Scheduler s;
    KqueueDriver io{s};
    s.set_io_driver(&io);

    SocketPair sp;
    OneShot h{&s, sp.a};
    arm(h.task, one_shot_read, &h);

    io.wait_readable(sp.a, &h.task);
    send_byte(sp.b);

    s.run();

    CHECK(h.reads == 1);
    CHECK(h.bytes == 1);
}

TEST_CASE("a handler can re-arm and be woken repeatedly") {
    Scheduler s;
    KqueueDriver io{s};
    s.set_io_driver(&io);

    SocketPair sp;
    Repeater r{&s, &io, sp.a, sp.b, 4};
    arm(r.task, repeat_read, &r);

    io.wait_readable(sp.a, &r.task);
    send_byte(sp.b);

    s.run();

    CHECK(r.reads == 4);
}

// The next two drive poll() by hand and leave the scheduler without a driver,
// so run() simply drains whatever poll spawned and returns.

TEST_CASE("forget cancels a registration before it fires") {
    Scheduler s;
    KqueueDriver io{s};

    SocketPair sp;
    Flag f;
    arm(f.task, set_flag, &f);

    io.wait_readable(sp.a, &f.task);
    io.forget(sp.a);
    send_byte(sp.b);        // readable, but nobody is listening any more

    io.poll(millis(50));
    s.run();

    CHECK(f.fired == false);
}

TEST_CASE("only the fd that became readable fires") {
    Scheduler s;
    KqueueDriver io{s};

    SocketPair one, two;
    Flag f1, f2;
    arm(f1.task, set_flag, &f1);
    arm(f2.task, set_flag, &f2);

    io.wait_readable(one.a, &f1.task);
    io.wait_readable(two.a, &f2.task);
    send_byte(two.b);

    io.poll(millis(50));
    s.run();

    CHECK(f1.fired == false);
    CHECK(f2.fired == true);
}

TEST_CASE("readiness is one-shot: a fired registration does not fire again") {
    Scheduler s;
    KqueueDriver io{s};

    SocketPair sp;
    Flag f;
    arm(f.task, set_flag, &f);

    io.wait_readable(sp.a, &f.task);
    send_byte(sp.b);

    io.poll(millis(50));
    s.run();
    REQUIRE(f.fired == true);

    f.fired = false;
    send_byte(sp.b);        // still readable, but the registration is gone
    io.poll(millis(50));
    s.run();

    CHECK(f.fired == false);
}
