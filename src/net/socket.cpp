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
#include <sys/select.h>
#include <fcntl.h>
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

bool set_nonblocking(socket_t s) {
    u_long mode = 1;  // non-zero => non-blocking
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
}

std::ptrdiff_t send_some(socket_t s, const std::uint8_t* buf, std::size_t n) {
    if (n == 0) return 0;
    int r = ::send(s, reinterpret_cast<const char*>(buf), static_cast<int>(n), 0);
    if (r >= 0) return static_cast<std::ptrdiff_t>(r);
    return (WSAGetLastError() == WSAEWOULDBLOCK) ? 0 : -1;
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

bool set_nonblocking(socket_t s) {
    int fl = ::fcntl(s, F_GETFL, 0);
    if (fl < 0) return false;
    return ::fcntl(s, F_SETFL, fl | O_NONBLOCK) == 0;
}

std::ptrdiff_t send_some(socket_t s, const std::uint8_t* buf, std::size_t n) {
    if (n == 0) return 0;
    ssize_t r;
    do {
        r = ::send(s, static_cast<const void*>(buf), n, kSendFlags);
    } while (r < 0 && errno == EINTR);
    if (r >= 0) return static_cast<std::ptrdiff_t>(r);
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
}

#endif

bool send_all(socket_t s, const std::vector<std::uint8_t>& buf) {
    return send_all(s, buf.data(), buf.size());
}

// Portable select() readability wait. On Winsock the first arg (nfds) is ignored; on POSIX it must
// be the highest fd + 1. A negative timeout blocks indefinitely (nullptr timeval).
bool wait_readable(socket_t s, int timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(s, &rfds);
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    timeval* ptv = (timeout_ms < 0) ? nullptr : &tv;
#ifdef _WIN32
    int r = ::select(0, &rfds, nullptr, nullptr, ptv);
#else
    int r = ::select(static_cast<int>(s) + 1, &rfds, nullptr, nullptr, ptv);
#endif
    return r > 0 && FD_ISSET(s, &rfds);
}

// Portable multi-fd select() readability wait (layer 9). Watches {listener} u {clients} at once so
// a single-threaded broadcast server multiplexes joins (listener) and leaves (client EOF). On
// Winsock the first arg (nfds) is ignored; on POSIX it must be max(fd)+1. Negative timeout blocks.
bool select_readable(const std::vector<socket_t>& fds, int timeout_ms,
                     std::vector<socket_t>& ready) {
    ready.clear();
    if (fds.empty()) return false;
    fd_set rfds;
    FD_ZERO(&rfds);
#ifndef _WIN32
    socket_t maxfd = 0;
#endif
    for (socket_t s : fds) {
        if (!is_valid(s)) continue;
        FD_SET(s, &rfds);
#ifndef _WIN32
        if (s > maxfd) maxfd = s;
#endif
    }
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    timeval* ptv = (timeout_ms < 0) ? nullptr : &tv;
#ifdef _WIN32
    int r = ::select(0, &rfds, nullptr, nullptr, ptv);
#else
    int r = ::select(static_cast<int>(maxfd) + 1, &rfds, nullptr, nullptr, ptv);
#endif
    if (r <= 0) return false;
    for (socket_t s : fds)
        if (is_valid(s) && FD_ISSET(s, &rfds)) ready.push_back(s);
    return !ready.empty();
}

// Portable read+write select() (layer 11). One call watches readability (joins, leaves) AND
// writability (a slow client's kernel buffer opened up — flush its userspace send buffer). On
// Winsock nfds is ignored; on POSIX it is max(fd)+1 over BOTH sets. Negative timeout blocks.
bool select_rw(const std::vector<socket_t>& rfds_in, const std::vector<socket_t>& wfds_in,
               int timeout_ms, std::vector<socket_t>& readable, std::vector<socket_t>& writable) {
    readable.clear();
    writable.clear();
    if (rfds_in.empty() && wfds_in.empty()) return false;
    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
#ifndef _WIN32
    socket_t maxfd = 0;
#endif
    for (socket_t s : rfds_in) {
        if (!is_valid(s)) continue;
        FD_SET(s, &rfds);
#ifndef _WIN32
        if (s > maxfd) maxfd = s;
#endif
    }
    for (socket_t s : wfds_in) {
        if (!is_valid(s)) continue;
        FD_SET(s, &wfds);
#ifndef _WIN32
        if (s > maxfd) maxfd = s;
#endif
    }
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    timeval* ptv = (timeout_ms < 0) ? nullptr : &tv;
#ifdef _WIN32
    int r = ::select(0, &rfds, &wfds, nullptr, ptv);
#else
    int r = ::select(static_cast<int>(maxfd) + 1, &rfds, &wfds, nullptr, ptv);
#endif
    if (r <= 0) return false;
    for (socket_t s : rfds_in)
        if (is_valid(s) && FD_ISSET(s, &rfds)) readable.push_back(s);
    for (socket_t s : wfds_in)
        if (is_valid(s) && FD_ISSET(s, &wfds)) writable.push_back(s);
    return !readable.empty() || !writable.empty();
}

bool set_sndbuf(socket_t s, int bytes) {
#ifdef _WIN32
    return ::setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bytes),
                        sizeof(bytes)) == 0;
#else
    return ::setsockopt(s, SOL_SOCKET, SO_SNDBUF, static_cast<const void*>(&bytes),
                        sizeof(bytes)) == 0;
#endif
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
