#pragma once

#include "coro/intrusive_list.hpp"
#include "coro/task.hpp"

namespace coro {

class Scheduler {
public:
    Scheduler() noexcept = default;
    void spawn(Task* t) noexcept;
    void run();

private:
    IntrusiveList<Task, &Task::link> ready_;
};

}