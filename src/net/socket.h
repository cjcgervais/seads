// SEADS minimal blocking-TCP socket wrapper (netcode layer 7). Dependency-free (no asio/boost) —
// just BSD sockets on POSIX and Winsock2 on Windows, behind one #ifdef _WIN32 boundary. It exists
// only to move the layer-7 length-prefixed frame stream between two OS processes (or two threads in
// the loopback determinism bridge). It is TRANSPORT: outside the kernel + world_hash, no det_math.
//
// The wire is endian-NEUTRAL by construction (LEB128 varints), so payload bytes are never byte-
// swapped; only sin_port / sin_addr use network order (htons / INADDR_LOOPBACK). send_all loops
// until every byte is out (a single send() may be short on a large frame); recv_some returns
// whatever arrived (the StreamReassembler tolerates any chunk size, so no recv_all is needed).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace seads {
namespace netsock {

#ifdef _WIN32
using socket_t = SOCKET;
inline bool is_valid(socket_t s) { return s != INVALID_SOCKET; }
inline socket_t invalid_socket() { return INVALID_SOCKET; }
#else
using socket_t = int;
inline bool is_valid(socket_t s) { return s >= 0; }
inline socket_t invalid_socket() { return -1; }
#endif

// Refcounted Winsock init/teardown (WSAStartup/WSACleanup); a no-op on POSIX. RAII so the single-
// process loopback test can construct several without double-init. Construct one before any socket
// use; keep it alive for the socket's lifetime.
class WsaGuard {
public:
    WsaGuard();
    ~WsaGuard();
    WsaGuard(const WsaGuard&) = delete;
    WsaGuard& operator=(const WsaGuard&) = delete;
};

void close_socket(socket_t s);

// Send exactly `n` bytes (loops over short writes). Returns false on error/EOF before all sent.
bool send_all(socket_t s, const std::uint8_t* buf, std::size_t n);
bool send_all(socket_t s, const std::vector<std::uint8_t>& buf);

// Receive up to `cap` bytes into buf; returns the count read (>0), 0 on clean EOF, <0 on error.
std::ptrdiff_t recv_some(socket_t s, std::uint8_t* buf, std::size_t cap);

// --- server side ---
// Create a listening TCP socket bound to 127.0.0.1:port (port 0 => OS-assigned). SO_REUSEADDR is
// set before bind. On success returns a valid socket and sets `bound_port` to the actual port
// (read back via getsockname when port==0). Returns invalid_socket() on failure.
socket_t listen_loopback(std::uint16_t port, std::uint16_t& bound_port, int backlog = 1);

// Accept one connection (blocks). Returns the accepted socket or invalid_socket() on error.
socket_t accept_one(socket_t listener);

// --- client side ---
// Connect a TCP socket to 127.0.0.1:port (blocks). Returns the connected socket or invalid_socket().
socket_t connect_loopback(std::uint16_t port);

}  // namespace netsock
}  // namespace seads
