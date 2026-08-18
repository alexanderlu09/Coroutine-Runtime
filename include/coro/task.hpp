#pragma once

#include "coro/intrusive_list.hpp"

namespace coro {
struct Task {
    void (*fn)(void*) = nullptr;
    void* state = nullptr;
    ListNode link;  
};

}