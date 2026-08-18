#include "coro/scheduler.hpp"

namespace coro {

void Scheduler::spawn(Task* t) noexcept {
    ready_.push_back(t);
}

void Scheduler::run() {
    while(!ready_.empty()){
        Task* t = ready_.pop_front();
        t->fn(t->state);
    }
}

}