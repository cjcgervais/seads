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

// --- non-blocking helpers (netcode layer 8: multi-client fan-out) --------------------------
// Put a socket in non-blocking mode (ioctlsocket FIONBIO on Windows, fcntl O_NONBLOCK on POSIX).
// Returns false on error. Used so a fan-out server's accept() never wedges on an absent client.
bool set_nonblocking(socket_t s);

// Block up to `timeout_ms` for `s` to become readable (a pending connection on a listener, or
// data on a stream). Returns true iff readable within the timeout; false on timeout OR error —
// portable select() wrapper (Winsock ignores nfds; POSIX uses s+1). timeout_ms<0 => wait forever.
bool wait_readable(socket_t s, int timeout_ms);

// --- multi-fd readability select (netcode layer 9: single-thread broadcast event loop) -----
// Block up to `timeout_ms` for ANY socket in `fds` to become readable, filling `ready` with the
// subset that are. Returns true iff >=1 became ready; false on timeout OR error. One select() over
// {listener} u {clients} lets a single-threaded fan-out server multiplex accepting new clients
// (JOIN, listener readable) and reaping departed ones (LEAVE, a client readable then recv()==0)
// without a thread per connection. Empty `fds` => false. timeout_ms<0 => wait forever.
bool select_readable(const std::vector<socket_t>& fds, int timeout_ms,
                     std::vector<socket_t>& ready);

// --- read+write select (netcode layer 11: async single-thread output) ----------------------
// select_readable generalized with a WRITE set: blocks up to `timeout_ms` for any of `rfds` to
// become readable OR any of `wfds` to become writable, filling `readable`/`writable` with the ready
// subsets. Returns true iff >=1 socket (either set) became ready; false on timeout OR error. Lets
// the async broadcast server flush a slow client's userspace send buffer exactly when the kernel
// can accept more bytes, in the SAME select() that services joins/leaves. Both sets empty => false.
bool select_rw(const std::vector<socket_t>& rfds, const std::vector<socket_t>& wfds,
               int timeout_ms, std::vector<socket_t>& readable, std::vector<socket_t>& writable);

// One NON-BLOCKING send attempt (the socket must be non-blocking): returns the byte count the
// kernel accepted (>=0; 0 means the kernel send buffer is full right now — EWOULDBLOCK — try again
// on the next writability), or <0 on a fatal error (peer gone). The layer-11 building block: unlike
// send_all it NEVER blocks, so a slow client cannot back-pressure the broadcast frame loop.
std::ptrdiff_t send_some(socket_t s, const std::uint8_t* buf, std::size_t n);

// Pin a socket's kernel send/receive buffer to `bytes` (SO_SNDBUF / SO_RCVBUF; disables autotuning
// where the OS would otherwise grow it). On a LISTENER, accepted sockets inherit the value. Test
// instrumentation: the layer-11 bridge pins tiny kernel buffers so a blocking send provably wedges
// where the async path must not. Returns false on error.
bool set_sndbuf(socket_t s, int bytes);
bool set_rcvbuf(socket_t s, int bytes);

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
