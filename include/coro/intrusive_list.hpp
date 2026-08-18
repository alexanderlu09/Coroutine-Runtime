#pragma once

// Intrusive doubly-linked list: link pointers live inside the element, so
// queueing allocates nothing and erase is O(1) given only the element.
// Used later by ready queues (B4), wait lists (B13), timer buckets (B6).

#include <cstddef>

namespace coro {

struct ListNode {
    ListNode* prev = nullptr;
    ListNode* next = nullptr;
};

// Link names which ListNode inside T to use, e.g.
//   IntrusiveList<Task, &Task::link>
template <typename T, ListNode T::*Link>
class IntrusiveList {
public:
    IntrusiveList() noexcept{
        sentinel_.next = &sentinel_;
        sentinel_.prev = &sentinel_;
    };

    IntrusiveList(const IntrusiveList&) = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;

    bool empty() const noexcept;

    void push_back(T* elem) noexcept;
    void push_front(T* elem) noexcept;
    T* pop_front() noexcept;        // nullptr if empty
    void erase(T* elem) noexcept;   // elem must be in this list

    std::size_t size() const noexcept;  // O(n), tests only

private:
    static ListNode* to_node(T* elem) noexcept {
        return &(elem->*Link);
    };
    static T* to_elem(ListNode* node) noexcept {
        T* null_obj = nullptr;
        auto offset = reinterpret_cast<char*>(&(null_obj->*Link))
                      - reinterpret_cast<char*>(null_obj);
        return (reinterpret_cast<T*>(reinterpret_cast<char*>(node)-offset));
    };

    ListNode sentinel_;
};

// Definitions go here (template, so no .cpp).
template <typename T, ListNode T::*Link>
bool IntrusiveList<T, Link>::empty() const noexcept{
    return sentinel_.next == &sentinel_;
}

template <typename T, ListNode T::*Link>
void IntrusiveList<T, Link>::push_back(T* elem) noexcept{
    sentinel_.prev->next = to_node(elem);
    to_node(elem)->prev = sentinel_.prev;
    to_node(elem)->next = &sentinel_;
    sentinel_.prev = to_node(elem);
}

template <typename T, ListNode T::*Link>
void IntrusiveList<T, Link>::push_front(T* elem) noexcept{
    sentinel_.next->prev = to_node(elem);
    to_node(elem)->next = sentinel_.next;
    to_node(elem)->prev = &sentinel_;
    sentinel_.next = to_node(elem);
}

template <typename T, ListNode T::*Link>
T* IntrusiveList<T, Link>::pop_front() noexcept{
    if(empty()) return nullptr;
    T* tmp = to_elem(sentinel_.next);
    sentinel_.next = sentinel_.next->next;
    sentinel_.next->prev = &sentinel_;
    return tmp;
}

template <typename T, ListNode T::*Link>
void IntrusiveList<T, Link>::erase(T* elem) noexcept{
    to_node(elem)->prev->next = to_node(elem)->next;
    to_node(elem)->next->prev = to_node(elem)->prev;
}

template <typename T, ListNode T::*Link>
std::size_t IntrusiveList<T, Link>::size() const noexcept{
    ListNode* head = sentinel_.next;
    std::size_t count = 0;
    while(head != &sentinel_){
        count++;
        head = head->next;
    }
    return count;
}

}  // namespace coro
