#!/usr/bin/env python3
"""
authsig_ref.py — SEADS layer-27 ASYMMETRIC-SIGNATURE handshake REFERENCE: HELLO-003 (an Ed25519
signature over the server challenge) + the PubkeyTable (a verifying PUBLIC-KEY identity binding).

Layer 26 (authmac_ref.py) verified a client's identity with a symmetric keyed MAC (SipHash): the
server held a shared 128-bit SECRET per token and recomputed the MAC. That is strictly stronger
than layer 21's bare-token lookup — but the server still holds a secret capable of FORGING any
client's proof, so a roster leak (or a malicious server) can impersonate. Layer 27 closes that
last gap with an ASYMMETRIC signature:

  * The challenge is UNCHANGED from layer 26 — CHALLENGE-001, a fresh per-connection nonce sent
    DOWN as the first downstream framing frame (nonce = SipHash(session_key, accept_counter)).
    We reuse authmac_ref's CHALLENGE-001 codec + derive_nonce verbatim: only the *proof* becomes
    asymmetric, not the freshness mechanism.

  * HELLO-003 — the client's identity claim + Ed25519 signature, sent UP in response:

        HELLO-003 = [version:1 byte = 0x03] [ZigZag+LEB128 token]
                    [LEB128 siglen = 64] [64 raw signature bytes]

    sig = Ed25519_sign(private_seed[token], nonce_le || token_le) proves the client holds the
    PRIVATE key whose public key is enrolled for that token, and binds the proof to BOTH the
    challenge (freshness) and the claimed identity. token + siglen ride the sealed GEO-001
    ZigZag+LEB128 codec; the 64 signature bytes are raw (opaque) ⇒ no new codec primitive, no
    det_math. Version 0x03 keeps it distinct from layer 21's HELLO-001 (0x01) and layer 26's
    HELLO-002 (0x02), both frozen; each decoder rejects the others' versions.

  * PubkeyTable — the verifying roster, holding ONLY PUBLIC keys. enroll(token, seat, pubkey)
    registers a token's DESIGNATED seat and its 32-byte Ed25519 public key. authenticate(token,
    nonce, sig) returns:
        - the token's designated seat, when the token is enrolled, its seat is free, AND
          Ed25519_verify(pubkey, nonce||token, sig) succeeds (the signature verifies);
        - SPECTATOR (-1) otherwise — unknown token, double-login (seat held), OR a BAD signature
          (forgery / wrong key / stale nonce). A forgery is rejected into the EXACT same SPECTATOR
          class as an unknown token, so the layer-21/26 determinism story composes verbatim:
          authentication is an admission filter that never touches the CommandQueue.

THE HEADLINE over layer 26: the server holds NO secret capable of signing — only public keys. A
full roster leak (every enrolled public key) still cannot impersonate any client, because a valid
HELLO-003 requires the private seed the server never sees. This is real public-key identity.

Honest scope: keys are enrolled from a fixed roster (a production PKI would carry certificates /
key rotation); the challenge nonce is CSPRNG-seeded in production, a FIXED seed here for a
reproducible run. Ed25519 signing is deterministic, so the run is fully reproducible. The upstream
AUTHORIZATION predicate (seat_authorizes) is UNCHANGED from layers 18/21/26.

Usage:  python tools/authsig_ref.py        # internal self-test
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import geo001_ref as g
import bound_ref as bnd            # reuse SPECTATOR + seat_authorizes
import authmac_ref as am           # reuse CHALLENGE-001 codec + derive_nonce (freshness unchanged)
import ed25519_ref as ed
import siphash_ref as sip

HELLO3_VERSION = 0x03
SPECTATOR = bnd.SPECTATOR  # -1
_MASK64 = (1 << 64) - 1

# The challenge is identical to layer 26 — reuse it wholesale.
CHALLENGE_VERSION = am.CHALLENGE_VERSION
encode_challenge = am.encode_challenge
decode_challenge = am.decode_challenge
derive_nonce = am.derive_nonce


# ---- HELLO-003 codec (mirror of src/net/authsig001.cpp) --------------------------------
def encode_hello3(token, sig):
    """One HELLO-003 (token + Ed25519 signature) -> wire bytes. token via the sealed i64 codec;
    the signature is a length-prefixed raw byte blob (LEB128 length, then bytes)."""
    sig = bytes(sig)
    out = bytearray([HELLO3_VERSION])
    out += g.encode_i64(int(token))
    out += g.leb128_encode_u64(len(sig))
    out += sig
    return bytes(out)


def decode_hello3(data, pos=0):
    """wire bytes -> ({token, sig}, next_pos). Raises ValueError on a wrong/absent version byte
    or a truncated signature blob."""
    if pos >= len(data) or data[pos] != HELLO3_VERSION:
        raise ValueError("hello003: bad or missing version byte")
    pos += 1
    token, pos = g.decode_i64(data, pos)
    siglen, pos = g.leb128_decode_u64(data, pos)
    if pos + siglen > len(data):
        raise ValueError("hello003: truncated signature")
    sig = bytes(data[pos:pos + siglen])
    pos += siglen
    return {"token": token, "sig": sig}, pos


# ---- the message signed / verified -----------------------------------------------------
def challenge_message(nonce, token):
    """The bytes the client signs and the server verifies: nonce_le || token_le (16 bytes). The
    same message shape layer 26 MAC'd — freshness (nonce) + identity (token)."""
    return sip.u64_le(nonce) + sip.u64_le(token)


def sign_challenge(seed, nonce, token):
    """Client side: Ed25519-sign the challenge message with the private seed -> 64-byte sig."""
    return ed.sign(seed, challenge_message(nonce, token))


def verify_challenge(pubkey, nonce, token, sig):
    """Server side: verify the signature over the challenge message under the public key."""
    return ed.verify(pubkey, challenge_message(nonce, token), sig)


# ---- verifying PUBLIC-KEY roster (mirror of src/net/authsigserver.cpp) ------------------
class PubkeyTable:
    """token -> (designated seat, 32-byte Ed25519 PUBLIC key). Holds NO secret. authenticate(
    token, nonce, sig) hands back the seat only when the token is enrolled, its seat is free, AND
    the signature verifies under the public key; else SPECTATOR. release(seat) frees it so a
    reconnecting identity reclaims ITS seat. A forged / stale / unknown credential is rejected into
    the SAME SPECTATOR class — and, unlike layer 26, a leak of this whole table cannot forge."""

    def __init__(self, n_aircraft):
        self.n = n_aircraft
        self._roster = {}                       # token -> (seat, pubkey bytes)
        self._occupied = [False] * n_aircraft

    def enroll(self, token, seat, pubkey):
        if not (0 <= seat < self.n):
            raise ValueError("enroll: seat out of range")
        pubkey = bytes(pubkey)
        if len(pubkey) != 32:
            raise ValueError("enroll: pubkey must be 32 bytes")
        self._roster[int(token)] = (int(seat), pubkey)

    def authenticate(self, token, nonce, sig):
        ent = self._roster.get(int(token))
        if ent is None:                         # unknown identity: no aircraft
            return SPECTATOR
        seat, pubkey = ent
        if self._occupied[seat]:                # valid identity, seat already held (double-login)
            return SPECTATOR
        if not verify_challenge(pubkey, nonce, token, sig):  # forgery / wrong key / stale nonce
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
    session_k = (0x1122334455667788, 0x99AABBCCDDEEFF00)

    # HELLO-003 known encoding pin (asserted identically in the C++ bridge): token=7 (zz 0x0E),
    # siglen=2, sig=[0xAA,0xBB] -> [0x03, 0x0E, 0x02, 0xAA, 0xBB].
    if encode_hello3(7, bytes([0xAA, 0xBB])) != bytes([0x03, 0x0E, 0x02, 0xAA, 0xBB]):
        print(f"FAIL hello3 known encoding: {encode_hello3(7, bytes([0xAA,0xBB])).hex()}"); fails += 1

    # Round-trip HELLO-003 over tokens (incl. negative) and real 64-byte signatures.
    for token in (-5, 0, 7, 300, 1 << 40):
        seed = bytes([(token + i) & 0xFF for i in range(32)])
        sig = ed.sign(seed, b"msg")
        dec, pos = decode_hello3(encode_hello3(token, sig))
        if pos != len(encode_hello3(token, sig)) or dec["token"] != token or dec["sig"] != sig:
            print(f"FAIL hello3 round-trip token={token}"); fails += 1

    # Wrong version bytes -> hard errors; not interchangeable with HELLO-001 (0x01) / HELLO-002
    # (0x02) / CHALLENGE-001 (0x01).
    for bad in (0x00, 0x01, 0x02, 0xFF):
        try:
            decode_hello3(bytes([bad, 0x0E, 0x00])); print(f"FAIL hello3 version {bad} should raise"); fails += 1
        except ValueError:
            pass

    # CHALLENGE-001 reused verbatim from layer 26 (freshness unchanged): known encoding + freshness.
    if encode_challenge(7) != bytes([0x01, 0x0E]):
        print("FAIL reused challenge encoding"); fails += 1
    if derive_nonce(*session_k, 5) == derive_nonce(*session_k, 6):
        print("FAIL nonce not fresh per accept"); fails += 1

    # PubkeyTable: identity + PROOF binding, invariant to enrollment/auth order (token order !=
    # seat order), a fresh per-connection nonce, and a verifying signature. The server holds ONLY
    # public keys.
    #        token seat  private seed (client-side only; NEVER enrolled)
    roster = {100: (2, bytes([(0x10 + i) & 0xFF for i in range(32)])),
              200: (0, bytes([(0x20 + i) & 0xFF for i in range(32)])),
              300: (1, bytes([(0x30 + i) & 0xFF for i in range(32)]))}
    pt = PubkeyTable(3)
    for tok, (seat, seed) in roster.items():
        pt.enroll(tok, seat, ed.public_key(seed))   # only the PUBLIC key is enrolled

    def present(tbl, token, seed, counter):
        nonce = derive_nonce(*session_k, counter)
        sig = sign_challenge(seed, nonce, token)
        return tbl.authenticate(token, nonce, sig)

    # each identity signs its distinct challenge and draws ITS roster seat — order != enrollment.
    if present(pt, 300, roster[300][1], 0) != 1: print("FAIL 300->seat1"); fails += 1
    if present(pt, 100, roster[100][1], 1) != 2: print("FAIL 100->seat2"); fails += 1
    if present(pt, 200, roster[200][1], 2) != 0: print("FAIL 200->seat0"); fails += 1
    if sorted(pt.occupied()) != [0, 1, 2]:
        print(f"FAIL occupancy: {pt.occupied()}"); fails += 1

    # FORGERY: the correct token, but a WRONG private key -> bad signature -> SPECTATOR. This is
    # the headline: even holding the server's ENTIRE public roster, an attacker cannot sign.
    pt2 = PubkeyTable(3)
    pt2.enroll(100, 2, ed.public_key(roster[100][1]))
    nonce = derive_nonce(*session_k, 0)
    wrong_seed = bytes([(0x99 + i) & 0xFF for i in range(32)])
    forged = sign_challenge(wrong_seed, nonce, 100)   # attacker's own key, claiming token 100
    if pt2.authenticate(100, nonce, forged) != SPECTATOR:
        print("FAIL forged signature (wrong key) should be rejected"); fails += 1
    if pt2.occupied() != []:
        print("FAIL forgery took a seat"); fails += 1

    # REPLAY: a valid signature captured for one nonce is rejected under a DIFFERENT nonce.
    good = sign_challenge(roster[100][1], nonce, 100)
    other_nonce = derive_nonce(*session_k, 99)
    if pt2.authenticate(100, other_nonce, good) != SPECTATOR:
        print("FAIL replayed signature (stale nonce) should be rejected"); fails += 1
    if pt2.authenticate(100, nonce, good) != 2:
        print("FAIL genuine holder should authenticate"); fails += 1

    # UNKNOWN token -> spectator; DOUBLE-LOGIN -> spectator; release then reclaim OWN seat.
    if present(pt, 999, roster[100][1], 3) != SPECTATOR:
        print("FAIL unknown token -> spectator"); fails += 1
    if present(pt, 100, roster[100][1], 4) != SPECTATOR:      # seat 2 held
        print("FAIL double-login -> spectator"); fails += 1
    pt.release(2)
    if present(pt, 100, roster[100][1], 5) != 2:              # same identity reclaims seat 2
        print("FAIL reconnect should reclaim own seat"); fails += 1

    # a spectator authorizes nothing; a seated client only its own seat (reused predicate).
    if bnd.seat_authorizes(SPECTATOR, 0) or not bnd.seat_authorizes(2, 2) or bnd.seat_authorizes(2, 0):
        print("FAIL authorization predicate under signature binding"); fails += 1

    if fails == 0:
        print("RESULT: HELLO-003 / PubkeyTable REFERENCE SELFTEST PASS")
        return 0
    print(f"RESULT: asymmetric-signature REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
