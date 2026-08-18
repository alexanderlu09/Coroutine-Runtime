#include "doctest/doctest.h"

#include "coro/intrusive_list.hpp"

#include <vector>

using namespace coro;

namespace {

// A stand-in for the Task type that block B2 will introduce.
struct Item {
    int value;
    ListNode link;
    ListNode other_link;  // proves an element can be in two lists at once
};

using ItemList = IntrusiveList<Item, &Item::link>;
using OtherList = IntrusiveList<Item, &Item::other_link>;

// Drain a list into a vector so tests can assert on order.
std::vector<int> drain(ItemList& list) {
    std::vector<int> out;
    while (Item* i = list.pop_front()) out.push_back(i->value);
    return out;
}

}  // namespace

TEST_CASE("a fresh list is empty") {
    ItemList list;
    CHECK(list.empty());
    CHECK(list.size() == 0);
    CHECK(list.pop_front() == nullptr);
}

TEST_CASE("push_back then pop_front is FIFO") {
    Item a{1, {}, {}}, b{2, {}, {}}, c{3, {}, {}};
    ItemList list;

    list.push_back(&a);
    CHECK_FALSE(list.empty());
    CHECK(list.size() == 1);

    list.push_back(&b);
    list.push_back(&c);
    CHECK(list.size() == 3);

    CHECK(drain(list) == std::vector<int>{1, 2, 3});
    CHECK(list.empty());
}

TEST_CASE("push_front then pop_front is LIFO") {
    Item a{1, {}, {}}, b{2, {}, {}}, c{3, {}, {}};
    ItemList list;

    list.push_front(&a);
    list.push_front(&b);
    list.push_front(&c);

    CHECK(drain(list) == std::vector<int>{3, 2, 1});
}

TEST_CASE("push_front and push_back interleave correctly") {
    Item a{1, {}, {}}, b{2, {}, {}}, c{3, {}, {}}, d{4, {}, {}};
    ItemList list;

    list.push_back(&b);
    list.push_front(&a);
    list.push_back(&c);
    list.push_front(&d);

    CHECK(drain(list) == std::vector<int>{4, 1, 2, 3});
}

TEST_CASE("erase from the middle") {
    Item a{1, {}, {}}, b{2, {}, {}}, c{3, {}, {}};
    ItemList list;
    list.push_back(&a);
    list.push_back(&b);
    list.push_back(&c);

    list.erase(&b);

    CHECK(list.size() == 2);
    CHECK(drain(list) == std::vector<int>{1, 3});
}

TEST_CASE("erase from the head and the tail") {
    Item a{1, {}, {}}, b{2, {}, {}}, c{3, {}, {}};
    ItemList list;
    list.push_back(&a);
    list.push_back(&b);
    list.push_back(&c);

    list.erase(&a);
    CHECK(drain(list) == std::vector<int>{2, 3});

    ItemList list2;
    list2.push_back(&a);
    list2.push_back(&b);
    list2.push_back(&c);

    list2.erase(&c);
    CHECK(drain(list2) == std::vector<int>{1, 2});
}

TEST_CASE("erase the only element leaves a valid empty list") {
    Item a{1, {}, {}};
    ItemList list;
    list.push_back(&a);
    list.erase(&a);

    CHECK(list.empty());
    CHECK(list.size() == 0);
    CHECK(list.pop_front() == nullptr);

    // The list must still work after being emptied by erase. If your sentinel
    // wiring is wrong, this is where it shows up.
    list.push_back(&a);
    CHECK(list.size() == 1);
    CHECK(list.pop_front() == &a);
}

TEST_CASE("an element can be in two lists at once") {
    // This is why the link is a template parameter rather than a base class.
    Item a{1, {}, {}}, b{2, {}, {}};
    ItemList ready;
    OtherList timers;

    ready.push_back(&a);
    ready.push_back(&b);
    timers.push_back(&b);

    CHECK(ready.size() == 2);
    CHECK(timers.size() == 1);

    // Removing from one list must not disturb the other.
    ready.erase(&b);
    CHECK(ready.size() == 1);
    CHECK(timers.size() == 1);
    CHECK(timers.pop_front() == &b);
}

TEST_CASE("stress: many elements, interleaved erase and pop") {
    constexpr int N = 1000;
    std::vector<Item> items(N);
    for (int i = 0; i < N; ++i) items[static_cast<std::size_t>(i)].value = i;

    ItemList list;
    for (auto& i : items) list.push_back(&i);
    CHECK(list.size() == N);

    // Erase every even value.
    for (int i = 0; i < N; i += 2) list.erase(&items[static_cast<std::size_t>(i)]);
    CHECK(list.size() == N / 2);

    auto remaining = drain(list);
    REQUIRE(remaining.size() == N / 2);
    for (std::size_t k = 0; k < remaining.size(); ++k) {
        CHECK(remaining[k] == static_cast<int>(k) * 2 + 1);
    }
}
