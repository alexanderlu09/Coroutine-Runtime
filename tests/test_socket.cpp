#include "doctest/doctest.h"

#include "coro/socket.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>

using namespace coro;

namespace {

int raw_fd() {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) std::abort();
    return fd;
}

bool fd_is_open(int fd) { return ::fcntl(fd, F_GETFD) != -1; }

// Which port did the kernel actually give us?
std::uint16_t port_of(const Socket& s) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getsockname(s.fd(), reinterpret_cast<sockaddr*>(&addr), &len) != 0) std::abort();
    return ntohs(addr.sin_port);
}

// A plain blocking client connection to loopback, for driving the listener.
int connect_to(std::uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) std::abort();
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) std::abort();
    return fd;
}

}  // namespace

TEST_CASE("a default Socket owns nothing") {
    Socket s;
    CHECK(s.valid() == false);
    CHECK(s.fd() == -1);
}

TEST_CASE("the destructor closes the fd") {
    const int fd = raw_fd();
    REQUIRE(fd_is_open(fd));
    {
        Socket s{fd};
        CHECK(s.valid());
    }
    CHECK(fd_is_open(fd) == false);
}

TEST_CASE("close is idempotent") {
    const int fd = raw_fd();
    Socket s{fd};
    s.close();
    CHECK(s.valid() == false);
    s.close();                 // must not close fd again
    CHECK(s.valid() == false);
}

TEST_CASE("move construction transfers ownership") {
    const int fd = raw_fd();
    Socket a{fd};
    Socket b{std::move(a)};

    CHECK(b.fd() == fd);
    CHECK(a.valid() == false);  // a must not close it
    CHECK(fd_is_open(fd));
}

TEST_CASE("a moved-from Socket does not close the fd it gave away") {
    const int fd = raw_fd();
    {
        Socket b;
        {
            Socket a{fd};
            b = std::move(a);
        }                       // a dies here
        CHECK(fd_is_open(fd));  // still b's
    }
    CHECK(fd_is_open(fd) == false);
}

TEST_CASE("move assignment closes the fd it was already holding") {
    const int old_fd = raw_fd();
    const int new_fd = raw_fd();

    Socket a{old_fd};
    Socket b{new_fd};
    a = std::move(b);

    CHECK(fd_is_open(old_fd) == false);  // a's previous fd was leaked otherwise
    CHECK(a.fd() == new_fd);
    CHECK(fd_is_open(new_fd));
}

TEST_CASE("self-move-assignment does not close the fd") {
    const int fd = raw_fd();
    Socket a{fd};
    Socket& ref = a;
    a = std::move(ref);
    CHECK(fd_is_open(fd));
}

TEST_CASE("release gives up ownership without closing") {
    const int fd = raw_fd();
    Socket s{fd};
    CHECK(s.release() == fd);
    CHECK(s.valid() == false);
    CHECK(fd_is_open(fd));
    ::close(fd);
}

TEST_CASE("set_nonblocking sets O_NONBLOCK") {
    Socket s{raw_fd()};
    CHECK((::fcntl(s.fd(), F_GETFL) & O_NONBLOCK) == 0);
    s.set_nonblocking();
    CHECK((::fcntl(s.fd(), F_GETFL) & O_NONBLOCK) != 0);
}

TEST_CASE("listen_on returns a non-blocking listening socket") {
    Socket l = listen_on(0);
    REQUIRE(l.valid());
    CHECK((::fcntl(l.fd(), F_GETFL) & O_NONBLOCK) != 0);
    CHECK(port_of(l) != 0);
}

TEST_CASE("accept_one returns an invalid Socket when nobody is waiting") {
    Socket l = listen_on(0);
    Socket c = accept_one(l);
    CHECK(c.valid() == false);
}

TEST_CASE("accept_one returns a connected socket after a client connects") {
    Socket l = listen_on(0);
    const int client = connect_to(port_of(l));

    // connect() can return before the connection lands in the listener's accept
    // queue, so poll rather than assuming it is there on the first try.
    Socket server;
    for (int i = 0; i < 1000 && !server.valid(); ++i) server = accept_one(l);
    REQUIRE(server.valid());
    CHECK(server.fd() != l.fd());
    CHECK((::fcntl(server.fd(), F_GETFL) & O_NONBLOCK) != 0);

    // The two ends are actually connected.
    const char out = 'z';
    REQUIRE(::write(client, &out, 1) == 1);
    char in = 0;
    // The server side is non-blocking; loopback delivery is immediate but not
    // instantaneous, so retry on EAGAIN.
    ssize_t n = -1;
    for (int i = 0; i < 1000 && n < 0; ++i) n = ::read(server.fd(), &in, 1);
    CHECK(n == 1);
    CHECK(in == 'z');

    ::close(client);
}
