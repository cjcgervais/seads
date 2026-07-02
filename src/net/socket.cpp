// SEADS minimal blocking-TCP socket wrapper (netcode layer 7). Dependency-free BSD sockets /
// Winsock2 behind one #ifdef _WIN32 boundary. See socket.h for the contract.
#include "socket.h"

#include <cstring>  // std::memset (used in the shared setup helpers)

#ifdef _WIN32
// winsock2.h is included by socket.h; link ws2_32 (CMake).
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace seads {
namespace netsock {

#ifdef _WIN32

WsaGuard::WsaGuard() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
}
WsaGuard::~WsaGuard() { WSACleanup(); }

void close_socket(socket_t s) {
    if (is_valid(s)) closesocket(s);
}

bool send_all(socket_t s, const std::uint8_t* buf, std::size_t n) {
    std::size_t sent = 0;
    while (sent < n) {
        int chunk = static_cast<int>(n - sent);
        int r = ::send(s, reinterpret_cast<const char*>(buf + sent), chunk, 0);
        if (r <= 0) return false;
        sent += static_cast<std::size_t>(r);
    }
    return true;
}

std::ptrdiff_t recv_some(socket_t s, std::uint8_t* buf, std::size_t cap) {
    int r = ::recv(s, reinterpret_cast<char*>(buf), static_cast<int>(cap), 0);
    return static_cast<std::ptrdiff_t>(r);
}

#else  // ---------------- POSIX ----------------

WsaGuard::WsaGuard() {}
WsaGuard::~WsaGuard() {}

void close_socket(socket_t s) {
    if (is_valid(s)) ::close(s);
}

#ifdef MSG_NOSIGNAL
static const int kSendFlags = MSG_NOSIGNAL;  // avoid SIGPIPE on a dead peer (Linux)
#else
static const int kSendFlags = 0;             // macOS uses SO_NOSIGPIPE / SIG_IGN instead
#endif

bool send_all(socket_t s, const std::uint8_t* buf, std::size_t n) {
    std::size_t sent = 0;
    while (sent < n) {
        ssize_t r = ::send(s, static_cast<const void*>(buf + sent), n - sent, kSendFlags);
        if (r <= 0) {
            if (r < 0 && errno == EINTR) continue;
            return false;
        }
        sent += static_cast<std::size_t>(r);
    }
    return true;
}

std::ptrdiff_t recv_some(socket_t s, std::uint8_t* buf, std::size_t cap) {
    ssize_t r;
    do {
        r = ::recv(s, static_cast<void*>(buf), cap, 0);
    } while (r < 0 && errno == EINTR);
    return static_cast<std::ptrdiff_t>(r);
}

#endif

bool send_all(socket_t s, const std::vector<std::uint8_t>& buf) {
    return send_all(s, buf.data(), buf.size());
}

// --- shared (portable) setup helpers, using the type aliases above --------------------
socket_t listen_loopback(std::uint16_t port, std::uint16_t& bound_port, int backlog) {
    socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!is_valid(s)) return invalid_socket();

    int yes = 1;
#ifdef _WIN32
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, static_cast<const void*>(&yes), sizeof(yes));
#endif

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(s);
        return invalid_socket();
    }
    if (::listen(s, backlog) != 0) {
        close_socket(s);
        return invalid_socket();
    }
    // read back the OS-assigned port (matters when port==0)
    sockaddr_in bound;
    std::memset(&bound, 0, sizeof(bound));
#ifdef _WIN32
    int blen = static_cast<int>(sizeof(bound));
#else
    socklen_t blen = sizeof(bound);
#endif
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&bound), &blen) != 0) {
        close_socket(s);
        return invalid_socket();
    }
    bound_port = ntohs(bound.sin_port);
    return s;
}

socket_t accept_one(socket_t listener) {
    socket_t c = ::accept(listener, nullptr, nullptr);
    return c;
}

socket_t connect_loopback(std::uint16_t port) {
    socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!is_valid(s)) return invalid_socket();
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(s);
        return invalid_socket();
    }
    return s;
}

}  // namespace netsock
}  // namespace seads
