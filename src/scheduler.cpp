#include "coro/scheduler.hpp"

namespace coro {

void Scheduler::spawn(Task* t) noexcept {
    ready_.push_back(t);
}

void Scheduler::stop() noexcept {
    running_ = false;
}

void Scheduler::set_io_driver(IoDriver* io) noexcept {
    io_ = io;
}

void Scheduler::run() {
    running_ = true;
    while(running_){
        while(!ready_.empty()){
            Task* t = ready_.pop_front();
            t->fn(t->state);
        }
        if(io_ == nullptr) break;
        io_->poll(seconds(1));
    }   
}

}