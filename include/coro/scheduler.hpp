#pragma once

#include "coro/intrusive_list.hpp"
#include "coro/task.hpp"
#include "coro/io_driver.hpp"

namespace coro {

class Scheduler {
public:
    Scheduler() noexcept = default;
    void spawn(Task* t) noexcept;
    void run();
    void set_io_driver(IoDriver* io) noexcept;
    void stop() noexcept;
private:
    IntrusiveList<Task, &Task::link> ready_;
    IoDriver* io_ = nullptr;
    bool running_ = true;
};

}