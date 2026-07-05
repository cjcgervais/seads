#!/usr/bin/env python3
"""
authmac_ref.py — SEADS layer-26 STRONG-CREDENTIAL handshake REFERENCE: CHALLENGE-001 +
HELLO-002 records + the SecretTable (verifying keyed-MAC identity binding).

Layer 21 (auth_ref.py) bound a client's seat to its IDENTITY, but the "credential" was an
abstracted i64 `token` the server merely LOOKED UP — a public username with no proof of
possession. Anyone who observed or guessed a token could claim that identity and take its
seat. Every auth ADR flagged this as the honest-scope gap: "a production system would carry a
MAC/signature the server verifies against a secret." Layer 26 closes it.

The credential becomes a keyed MAC over a server-issued CHALLENGE, verified against a
pre-shared secret. Two transport records (both pure metadata, OUTSIDE the kernel/world_hash —
no det_math, no seal, rides v1.26r0), plus a SecretTable:

  * CHALLENGE-001 — the server's one-time challenge, sent DOWN as the FIRST downstream framing
    frame the instant a client connects (BEFORE the client's HELLO). It carries a fresh nonce:

        CHALLENGE-001 = [version:1 byte = 0x01] [ZigZag+LEB128 nonce]

    The server derives nonce = SipHash(session_key, accept_counter) — a CSPRNG-seeded session
    key in production; a FIXED seed in the deterministic bridge so the run is reproducible.
    Each accepted connection gets a distinct nonce, so a captured HELLO cannot be replayed to a
    later connection (the fresh challenge won't match).

  * HELLO-002 — the client's identity claim + proof, sent UP in response to the challenge:

        HELLO-002 = [version:1 byte = 0x02] [ZigZag+LEB128 token] [ZigZag+LEB128 mac]

    token is the (public) identity; mac = SipHash(secret[token], nonce_le || token_le) proves
    the client holds the 128-bit secret bound to that token AND binds the proof to BOTH the
    challenge (freshness) and the claimed identity. The mac u64 is carried as an i64 bit
    pattern through the sealed GEO-001 ZigZag+LEB128 codec ⇒ no new primitive, no det_math.
    (Version 0x02 distinguishes it from layer 21's HELLO-001 token-only record, which stays
    byte-for-byte frozen; a v1-only decoder rejects a v2 record and vice-versa.)

  * SecretTable — the verifying roster. enroll(token, seat, k0, k1) registers a token's
    DESIGNATED seat and its 128-bit secret. authenticate(token, nonce, mac) returns:
        - the token's designated seat, when the token is enrolled, its seat is free, AND the
          presented mac EQUALS SipHash(secret, nonce||token) (the proof verifies);
        - SPECTATOR (-1) otherwise — unknown token, double-login (seat held), OR a BAD MAC
          (forgery / wrong secret / stale nonce). A forgery is rejected into the EXACT same
          SPECTATOR class as an unknown token, so the layer-21 determinism story composes
          verbatim: authentication is an admission filter that never touches the CommandQueue.

The headline vs layer 21: a client must now PROVE possession of a secret, not merely name a
token. Honest scope (the named next step): this is a symmetric MAC (shared secret, server
verifies) with a server-contributed nonce for freshness; an asymmetric SIGNATURE (public-key
identity, no shared secret on the server) is the further hardening, and a production nonce must
come from a real CSPRNG (the bridge pins a seed only to stay reproducible). The upstream
AUTHORIZATION predicate (seat_authorizes) is UNCHANGED from layers 18/21.

Usage:  python tools/authmac_ref.py        # internal self-test
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import geo001_ref as g
import bound_ref as bnd            # reuse SPECTATOR + seat_authorizes
import siphash_ref as sip

CHALLENGE_VERSION = 0x01
HELLO2_VERSION = 0x02
SPECTATOR = bnd.SPECTATOR  # -1
_MASK64 = (1 << 64) - 1


# ---- CHALLENGE-001 codec (mirror of src/net/authmac001.cpp) ----------------------------
def encode_challenge(nonce):
    """One CHALLENGE-001 nonce -> wire bytes."""
    out = bytearray([CHALLENGE_VERSION])
    out += g.encode_i64(int(nonce) if int(nonce) < (1 << 63) else int(nonce) - (1 << 64))
    return bytes(out)


def decode_challenge(data, pos=0):
    """wire bytes -> (nonce_u64, next_pos). Raises ValueError on a wrong/absent version byte."""
    if pos >= len(data) or data[pos] != CHALLENGE_VERSION:
        raise ValueError("challenge001: bad or missing version byte")
    pos += 1
    nonce_i, pos = g.decode_i64(data, pos)
    return nonce_i & _MASK64, pos


# ---- HELLO-002 codec (mirror of src/net/authmac001.cpp) --------------------------------
def encode_hello2(token, mac):
    """One HELLO-002 (token + keyed-MAC proof) -> wire bytes. mac is a u64 carried as its i64
    bit pattern through the sealed i64 codec."""
    out = bytearray([HELLO2_VERSION])
    out += g.encode_i64(int(token))
    m = int(mac) & _MASK64
    out += g.encode_i64(m if m < (1 << 63) else m - (1 << 64))
    return bytes(out)


def decode_hello2(data, pos=0):
    """wire bytes -> ({token, mac}, next_pos). Raises ValueError on a wrong/absent version byte."""
    if pos >= len(data) or data[pos] != HELLO2_VERSION:
        raise ValueError("hello002: bad or missing version byte")
    pos += 1
    token, pos = g.decode_i64(data, pos)
    mac_i, pos = g.decode_i64(data, pos)
    return {"token": token, "mac": mac_i & _MASK64}, pos


# ---- MAC over (nonce, token) -----------------------------------------------------------
def compute_mac(k0, k1, nonce, token):
    """The credential proof: SipHash(secret, nonce_le || token_le). Binds the proof to the
    server's fresh nonce (freshness) and the claimed token (identity)."""
    msg = sip.u64_le(nonce) + sip.u64_le(token)
    return sip.siphash24(k0, k1, msg)


def derive_nonce(session_k0, session_k1, counter):
    """A per-connection challenge nonce: SipHash(session_key, counter_le). Distinct per accept
    ⇒ a captured HELLO cannot be replayed to a later connection."""
    return sip.siphash24(session_k0, session_k1, sip.u64_le(counter))


# ---- verifying credential roster (mirror of src/net/authmacserver.cpp) -----------------
class SecretTable:
    """token -> (designated seat, 128-bit secret). authenticate(token, nonce, mac) hands back
    the seat only when the token is enrolled, its seat is free, AND the mac verifies; else
    SPECTATOR. release(seat) frees it so a reconnecting identity reclaims ITS seat. A forged /
    stale / unknown credential is rejected into the SAME SPECTATOR class."""

    def __init__(self, n_aircraft):
        self.n = n_aircraft
        self._roster = {}                       # token -> (seat, k0, k1)
        self._occupied = [False] * n_aircraft

    def enroll(self, token, seat, k0, k1):
        if not (0 <= seat < self.n):
            raise ValueError("enroll: seat out of range")
        self._roster[int(token)] = (int(seat), int(k0) & _MASK64, int(k1) & _MASK64)

    def authenticate(self, token, nonce, mac):
        ent = self._roster.get(int(token))
        if ent is None:                         # unknown identity: no aircraft
            return SPECTATOR
        seat, k0, k1 = ent
        if self._occupied[seat]:                # valid identity, seat already held (double-login)
            return SPECTATOR
        expect = compute_mac(k0, k1, nonce, token)
        if (int(mac) & _MASK64) != expect:      # forgery / wrong secret / stale nonce
            return SPECTATOR
        self._occupied[seat] = True
        return seat

    def release(self, seat):
        if 0 <= seat < self.n:
            self._occupied[seat] = False

    def occupied(self):
        return [i for i in range(self.n) if self._occupied[i]]


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0

    # CHALLENGE-001 known encoding pin (asserted identically in the C++ bridge): nonce=7 -> zz 14.
    if encode_challenge(7) != bytes([0x01, 0x0E]):
        print(f"FAIL challenge known encoding: {encode_challenge(7).hex()}"); fails += 1
    # HELLO-002 known encoding pin: version 0x02, token=7 (zz 0x0E), mac=1 (zz 0x02).
    if encode_hello2(7, 1) != bytes([0x02, 0x0E, 0x02]):
        print(f"FAIL hello2 known encoding: {encode_hello2(7, 1).hex()}"); fails += 1

    # Round-trip challenge nonces (incl. a full-width u64) and hello (token incl. negative + big mac).
    for nonce in (0, 1, 7, 300, (1 << 64) - 1, 0xA129CA6149BE45E5):
        dec, pos = decode_challenge(encode_challenge(nonce))
        if pos != len(encode_challenge(nonce)) or dec != (nonce & _MASK64):
            print(f"FAIL challenge round-trip nonce={nonce}: {dec}"); fails += 1
    for token, mac in ((-5, 0), (-1, (1 << 64) - 1), (0, 1), (300, 0xDEADBEEFFEEDFACE), (1 << 40, 42)):
        dec, pos = decode_hello2(encode_hello2(token, mac))
        if pos != len(encode_hello2(token, mac)) or dec["token"] != token or dec["mac"] != (mac & _MASK64):
            print(f"FAIL hello2 round-trip token={token} mac={mac}: {dec}"); fails += 1

    # Wrong version bytes -> hard errors; the two records are NOT interchangeable, and neither is a
    # layer-21 HELLO-001 (version 0x01 with only a token). decode_challenge accepts 0x01; that is the
    # CHALLENGE record, a different shape than HELLO-002 (0x02).
    for fn, bad in ((decode_challenge, bytes([0x02, 0x0E])), (decode_hello2, bytes([0x01, 0x0E]))):
        try:
            fn(bad); print(f"FAIL {fn.__name__} wrong version should raise"); fails += 1
        except ValueError:
            pass

    # SecretTable: identity + PROOF binding, invariant to enrollment/auth order (token order !=
    # seat order), a fresh per-connection nonce, and a verifying MAC.
    session_k = (0x1122334455667788, 0x99AABBCCDDEEFF00)
    st = SecretTable(3)
    #        token  seat  secret (k0, k1)
    st.enroll(100, 2, 0xA1, 0xA2)
    st.enroll(200, 0, 0xB1, 0xB2)
    st.enroll(300, 1, 0xC1, 0xC2)

    # each identity gets a distinct challenge, computes its proof, and draws ITS roster seat —
    # in an order DIFFERENT from enrollment.
    def present(tbl, token, k0, k1, counter):
        nonce = derive_nonce(session_k[0], session_k[1], counter)
        mac = compute_mac(k0, k1, nonce, token)
        return tbl.authenticate(token, nonce, mac)

    if present(st, 300, 0xC1, 0xC2, 0) != 1: print("FAIL 300->seat1"); fails += 1
    if present(st, 100, 0xA1, 0xA2, 1) != 2: print("FAIL 100->seat2"); fails += 1
    if present(st, 200, 0xB1, 0xB2, 2) != 0: print("FAIL 200->seat0"); fails += 1
    if sorted(st.occupied()) != [0, 1, 2]:
        print(f"FAIL occupancy: {st.occupied()}"); fails += 1

    # Distinct challenges: two different accept counters -> different nonces (freshness).
    if derive_nonce(session_k[0], session_k[1], 5) == derive_nonce(session_k[0], session_k[1], 6):
        print("FAIL nonce not fresh per accept"); fails += 1

    # FORGERY: correct token, but a WRONG secret -> bad MAC -> SPECTATOR (the headline over layer 21,
    # where knowing the token alone was enough).
    st2 = SecretTable(3)
    st2.enroll(100, 2, 0xA1, 0xA2)
    nonce = derive_nonce(session_k[0], session_k[1], 0)
    forged = compute_mac(0xBAD, 0xBAD, nonce, 100)      # attacker guesses token 100, wrong secret
    if st2.authenticate(100, nonce, forged) != SPECTATOR:
        print("FAIL forged MAC (wrong secret) should be rejected"); fails += 1
    # REPLAY: a valid MAC captured for one nonce is rejected under a DIFFERENT nonce.
    good = compute_mac(0xA1, 0xA2, nonce, 100)
    other_nonce = derive_nonce(session_k[0], session_k[1], 99)
    if st2.authenticate(100, other_nonce, good) != SPECTATOR:
        print("FAIL replayed MAC (stale nonce) should be rejected"); fails += 1
    # the genuine holder still authenticates under the right nonce.
    if st2.authenticate(100, nonce, good) != 2:
        print("FAIL genuine holder should authenticate"); fails += 1

    # UNKNOWN token -> spectator; DOUBLE-LOGIN -> spectator; release then reclaim OWN seat.
    if present(st, 999, 0, 0, 3) != SPECTATOR:
        print("FAIL unknown token -> spectator"); fails += 1
    if present(st, 100, 0xA1, 0xA2, 4) != SPECTATOR:      # seat 2 held
        print("FAIL double-login -> spectator"); fails += 1
    st.release(2)
    if present(st, 100, 0xA1, 0xA2, 5) != 2:              # same identity reclaims seat 2
        print("FAIL reconnect should reclaim own seat"); fails += 1

    # a spectator authorizes nothing; a seated client only its own seat (reused predicate).
    if bnd.seat_authorizes(SPECTATOR, 0) or not bnd.seat_authorizes(2, 2) or bnd.seat_authorizes(2, 0):
        print("FAIL authorization predicate under MAC binding"); fails += 1

    if fails == 0:
        print("RESULT: CHALLENGE-001 / HELLO-002 / SecretTable REFERENCE SELFTEST PASS")
        return 0
    print(f"RESULT: strong-credential REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
