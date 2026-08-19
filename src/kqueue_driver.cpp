#include "coro/kqueue_driver.hpp"

#include "coro/scheduler.hpp"

#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>

namespace coro {
namespace {

constexpr int kMaxEvents = 64;

}

KqueueDriver::KqueueDriver(Scheduler& sched) : sched_(sched) {
    kq_ = kqueue();
    if (kq_ < 0) abort();
}

KqueueDriver::~KqueueDriver() {
    if (kq_ >= 0) close(kq_);
}

void KqueueDriver::wait_readable(int fd, Task* t) {
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, t);
    if (kevent(kq_, &ev, 1, nullptr, 0, nullptr) < 0) abort();
}

void KqueueDriver::forget(int fd) {
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    (void)kevent(kq_, &ev, 1, nullptr, 0, nullptr);
}

void KqueueDriver::poll(Nanos timeout) {
    struct timespec ts;
    ts.tv_sec  = static_cast<time_t>(timeout / 1'000'000'000ULL);
    ts.tv_nsec = static_cast<long>(timeout % 1'000'000'000ULL);

    struct kevent events[kMaxEvents];
    const int n = kevent(kq_, nullptr, 0, events, kMaxEvents, &ts);
    if (n < 0) {
        if (errno == EINTR) return; 
        abort();
    }

    for (int i = 0; i < n; ++i) {
        auto* t = static_cast<Task*>(events[i].udata);
        if (t != nullptr) sched_.spawn(t);
    }
}

}  // namespace coro
