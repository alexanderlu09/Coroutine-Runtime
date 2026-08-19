#pragma once
#include "coro/io_driver.hpp"

namespace coro {

class Scheduler;

class KqueueDriver final : public IoDriver {
public:
    explicit KqueueDriver(Scheduler& sched);
    ~KqueueDriver() override;

    KqueueDriver(const KqueueDriver&) = delete;
    KqueueDriver& operator=(const KqueueDriver&) = delete;

    void wait_readable(int fd, Task* t) override;
    void forget(int fd) override;
    void poll(Nanos timeout) override;

private:
    Scheduler& sched_;
    int kq_ = -1;
};


}

