//
// DAY ONE EXERCISE -- do this before writing any runtime code.
//
// Goal: stop believing coroutines are magic. A coroutine frame is a heap-
// allocated struct, and `resume()` is an indirect call into a switch statement.
// You already know how to read the assembly for that; this file lets you.
//
// Build and run:
//     cmake --build --preset debug && ./build/debug/examples/coro_anatomy
//
// Then disassemble it:
//     ./scripts/disasm.sh
//
// Things to actually do (about 20 minutes, and worth every one of them):
//
//   1. Run it. Note the frame size printed by operator new.
//   2. Add `char scratch[1024];` as a local in counter() BEFORE the first
//      co_await. Rebuild. The frame grows by ~1024 bytes, because that local
//      is alive across a suspend point and therefore cannot live on the stack.
//   3. Now move that same array into a `{ }` block that opens and closes
//      entirely BETWEEN two co_awaits. Rebuild. The frame shrinks back --
//      the compiler proved it doesn't need to survive a suspension.
//      That is the liveness analysis from the 213 optimization lectures,
//      deciding storage placement.
//   4. In the disassembly, find `counter(int)` -- the "ramp". It is short:
//      it allocates the frame, stores two function pointers, and returns.
//   5. Find the resume function. Find the `switch` on the suspend index
//      (a compare + jump table, or a chain of compares at -O0).
//   6. Note that the frame's first two words are the resume and destroy
//      function pointers. `handle.resume()` is: load word at [frame+0],
//      indirect call. That's the ~2ns figure the entire project is built on.
//
#include <coroutine>
#include <cstdio>
#include <cstdlib>

struct Trace {
    struct promise_type {
        // Overriding these is what lets us see the frame size. The runtime
        // will later route this to a per-worker arena (block B11) instead of
        // malloc -- this printf is a preview of the allocation we will be
        // trying to make cheap.
        static void* operator new(std::size_t n) {
            std::printf("    [frame] allocated %zu bytes\n", n);
            return std::malloc(n);
        }
        static void operator delete(void* p) noexcept {
            std::printf("    [frame] freed\n");
            std::free(p);
        }

        Trace get_return_object() {
            return Trace{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // suspend_always here means the coroutine does NOT start running when
        // you call it -- it starts when you first resume(). That is what lets
        // a scheduler decide when and on which thread the body runs.
        std::suspend_always initial_suspend() noexcept { return {}; }

        // suspend_always here means the frame is NOT destroyed when the body
        // finishes; we destroy it explicitly. Change this to suspend_never and
        // watch the use-after-free in step 6 of the exercise below.
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}
        void unhandled_exception() { std::abort(); }
    };

    std::coroutine_handle<promise_type> h;

    ~Trace() { if (h) h.destroy(); }

    // Move-only: two owners of one frame is a double free.
    Trace(const Trace&) = delete;
    Trace& operator=(const Trace&) = delete;
    Trace(Trace&& o) noexcept : h(o.h) { o.h = {}; }

    explicit Trace(std::coroutine_handle<promise_type> hh) : h(hh) {}
};

static Trace counter(int limit) {
    // `i` is alive across a co_await, so it lives in the frame, not the stack.
    for (int i = 0; i < limit; ++i) {
        std::printf("    body: i=%d\n", i);
        co_await std::suspend_always{};
    }
    std::printf("    body: done\n");
}

int main() {
    std::printf("1. calling counter(3) -- the ramp runs, the body does NOT\n");
    Trace t = counter(3);

    std::printf("2. the handle is just a pointer: %p (sizeof = %zu)\n",
                t.h.address(), sizeof(t.h));

    std::printf("3. resuming until done\n");
    int resumes = 0;
    while (!t.h.done()) {
        t.h.resume();
        ++resumes;
    }
    std::printf("4. took %d resumes; done() == %s\n",
                resumes, t.h.done() ? "true" : "false");

    std::printf("5. destroying (frame is freed here, not when the body ended)\n");
    // ~Trace does this.
    return 0;
}
