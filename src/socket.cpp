#include "coro/socket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdlib>

namespace coro {

void Socket::close() noexcept {
    if(fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
}

Socket::Socket(Socket&& other) noexcept {
    fd_ = other.fd_;
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if(this != &other){
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

Socket::~Socket(){
    close();
}

Socket::Socket(int fd) noexcept {
    fd_ = fd;
}

bool Socket::valid() const noexcept {
    return fd_ != -1;
}

int Socket::release() noexcept {
    int t = fd_;
    fd_ = -1;
    return t;
}

int Socket::fd() const noexcept {
    return fd_;
}

void Socket::set_nonblocking() {
    int flags;
    if((flags = fcntl(fd_, F_GETFL)) < 0){
        abort();
    }
    flags |= O_NONBLOCK;
    fcntl(fd_, F_SETFL, flags);
}

Socket listen_on(std::uint16_t port) {
    int optval = 1;
    int socket_fd;
    if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        abort();
    }
    Socket s{socket_fd};
    if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0){
        abort();
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if(bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0){
        abort();
    }
    if(listen(socket_fd, SOMAXCONN) < 0){
        abort();
    }
    s.set_nonblocking();
    return s;
}

Socket accept_one(const Socket& listener) {
    int sock;
    if((sock = accept(listener.fd(), nullptr, nullptr)) == -1){
        if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNABORTED){
            return Socket{};
        }
        abort();
    }
    Socket s{sock};
    s.set_nonblocking();
    return s;
}

}