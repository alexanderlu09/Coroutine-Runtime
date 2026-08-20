#pragma once

#include <cstdint>

namespace coro {

class Socket {
public:
    Socket() noexcept = default;
    explicit Socket(int fd) noexcept;

    ~Socket();

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int  fd() const noexcept;
    bool valid() const noexcept;

    void close() noexcept;    // idempotent
    int  release() noexcept;  // hand the fd back, stop owning it

    void set_nonblocking();

private:
    int fd_ = -1;
};

// A non-blocking listening socket bound to `port` on all interfaces, with
// SO_REUSEADDR set. Pass 0 to let the kernel pick a free port. Aborts on
// failure, matching KqueueDriver.
Socket listen_on(std::uint16_t port);

// Accepts one pending connection. Returns an invalid Socket when nothing is
// waiting (accept gave EAGAIN) -- that is the normal case, not an error.
// The returned socket is non-blocking.
Socket accept_one(const Socket& listener);

}
