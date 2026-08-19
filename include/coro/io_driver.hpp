#pragma once

#include "coro/task.hpp"
#include "coro/clock.hpp"

namespace coro {

class IoDriver{
public:
    virtual ~IoDriver() = default;

    virtual void wait_readable(int fd, Task* t) = 0;
    virtual void forget(int fd) = 0;
    virtual void poll(Nanos timeout) = 0;
};

}