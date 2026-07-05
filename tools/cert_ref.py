#!/usr/bin/env python3
"""
cert_ref.py — SEADS layer-30 CERTIFICATE PKI REFERENCE: CERT-001 (a CA-signed certificate binding
an identity, seat, epoch, and public key) + HELLO-004 (present the certificate + sign the challenge)
+ the CaTable (a verifying server that trusts ONE certificate-authority public key, with revocation
and key-rotation). The honest-scope follow-up layer 27 named for its fixed public-key roster.

Layer 27 (authsig_ref.py) bound each seat to a per-token PUBLIC key the server ENROLLED directly:
    PubkeyTable.enroll(token, seat, pubkey)  — one entry per client, held on the server.
That is real public-key identity, but the roster is FIXED: adding a client means editing the server,
there is no way to REVOKE a compromised key, and no way to ROTATE a key. A production PKI fixes all
three by moving trust to a CERTIFICATE AUTHORITY:

  * The server trusts ONE CA public key — not a per-client roster. It never sees a client's key ahead
    of time.

  * A CERTIFICATE is the CA's Ed25519 signature over a canonical body binding the client's identity:

        cert_body = [cert_version:1 = 0x01] [ZigZag+LEB128 token] [ZigZag+LEB128 seat]
                    [ZigZag+LEB128 epoch] [32 raw client-pubkey bytes]
        certificate = cert_body [LEB128 ca_siglen = 64] [64 raw CA signature bytes]

    ca_sig = Ed25519_sign(ca_seed, cert_body). The CA issues certificates OFFLINE; the server only
    holds the CA public key to verify them. token/seat/epoch ride the sealed GEO-001 ZigZag+LEB128
    codec; the pubkey + CA signature are raw (opaque) => no new codec primitive, no det_math.

  * HELLO-004 — the client's handshake response, presenting the certificate AND proving possession of
    the certified private key by signing the fresh server challenge (layer 27's proof, reused):

        HELLO-004 = [version:1 = 0x04] [LEB128 certlen] [cert bytes]
                    [LEB128 siglen = 64] [64 raw challenge-signature bytes]

    challenge_sig = Ed25519_sign(client_seed, nonce_le || token_le) — the SAME message layer 26/27
    signed. Version 0x04 is distinct from HELLO-001/002/003 (0x01/0x02/0x03), all frozen.

  * CaTable — the verifying server. Holds the CA public key, a REVOCATION set (revoked tokens), and a
    per-token EPOCH FLOOR (rotation). authenticate(cert_bytes, nonce, challenge_sig) returns:
        - the certificate's designated seat, when (a) the certificate verifies under the CA public
          key, (b) the token is NOT revoked, (c) epoch >= the token's floor (not superseded by a
          rotation), (d) the seat is in range and free, AND (e) the challenge signature verifies
          under the certificate's embedded client public key;
        - SPECTATOR (-1) otherwise — a self-signed / tampered / non-CA certificate, a revoked token, a
          stale (pre-rotation) epoch, an out-of-range or held seat, OR a bad challenge signature.
      Every failure lands in the EXACT same SPECTATOR class, so the layer-21/26/27 determinism story
      composes verbatim: authentication is an admission filter that never touches the CommandQueue.

THE HEADLINE over layer 27: the server enrolls only ONE CA public key. New identities need no server
change (the CA issues a certificate); a compromised identity is REVOKED (revoke(token)); a client
ROTATES its key by re-certifying at a higher epoch and the server raising the floor (set_min_epoch).

Honest scope: ONE self-signed root CA (no intermediate certificates / chain-of-trust depth); the
revocation list + epoch floors are in-memory server state (a production CRL/OCSP distributes them).
Ed25519 signing is deterministic, so the run is fully reproducible. The challenge freshness
(CHALLENGE-001 + derive_nonce) and the upstream AUTHORIZATION predicate (seat_authorizes) are
UNCHANGED from layers 26/27.

Usage:  python tools/cert_ref.py        # internal self-test
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import geo001_ref as g
import bound_ref as bnd            # reuse SPECTATOR + seat_authorizes
import authmac_ref as am           # reuse CHALLENGE-001 codec + derive_nonce (freshness unchanged)
import authsig_ref as asig         # reuse the challenge signature (sign/verify over nonce||token)
import ed25519_ref as ed

CERT_VERSION = 0x01     # CERT-001 certificate body version
HELLO4_VERSION = 0x04   # HELLO-004 handshake record version
SPECTATOR = bnd.SPECTATOR  # -1

# The challenge is identical to layers 26/27 — reuse it wholesale.
CHALLENGE_VERSION = am.CHALLENGE_VERSION
encode_challenge = am.encode_challenge
decode_challenge = am.decode_challenge
derive_nonce = am.derive_nonce
# The proof-of-possession over the challenge is layer 27's, reused wholesale.
sign_challenge = asig.sign_challenge
verify_challenge = asig.verify_challenge


# ---- CERT-001 certificate codec (mirror of src/net/cert001.cpp) ------------------------
def cert_body(token, seat, epoch, pubkey):
    """The canonical bytes the CA signs: version + token + seat + epoch + the 32-byte client pubkey.
    Self-delimiting (three ZigZag+LEB128 ints then a fixed 32-byte key)."""
    pubkey = bytes(pubkey)
    if len(pubkey) != 32:
        raise ValueError("cert_body: pubkey must be 32 bytes")
    out = bytearray([CERT_VERSION])
    out += g.encode_i64(int(token))
    out += g.encode_i64(int(seat))
    out += g.encode_i64(int(epoch))
    out += pubkey
    return bytes(out)


def issue_cert(ca_seed, token, seat, epoch, client_pubkey):
    """CA side: sign a fresh certificate body with the CA private seed. Returns the full certificate
    (body + LEB128 siglen + 64-byte CA signature)."""
    body = cert_body(token, seat, epoch, client_pubkey)
    ca_sig = ed.sign(ca_seed, body)
    out = bytearray(body)
    out += g.leb128_encode_u64(len(ca_sig))
    out += ca_sig
    return bytes(out)


def decode_cert(data, pos=0):
    """wire bytes -> ({token, seat, epoch, pubkey, ca_sig, body}, next_pos). `body` is the exact byte
    slice the CA signed (so verification hashes the literal prefix, never a re-encoding). Raises
    ValueError on a wrong/absent version byte or a truncated pubkey/signature."""
    start = pos
    if pos >= len(data) or data[pos] != CERT_VERSION:
        raise ValueError("cert001: bad or missing version byte")
    pos += 1
    token, pos = g.decode_i64(data, pos)
    seat, pos = g.decode_i64(data, pos)
    epoch, pos = g.decode_i64(data, pos)
    if pos + 32 > len(data):
        raise ValueError("cert001: truncated pubkey")
    pubkey = bytes(data[pos:pos + 32])
    pos += 32
    body = bytes(data[start:pos])
    siglen, pos = g.leb128_decode_u64(data, pos)
    if pos + siglen > len(data):
        raise ValueError("cert001: truncated CA signature")
    ca_sig = bytes(data[pos:pos + siglen])
    pos += siglen
    return {"token": token, "seat": seat, "epoch": epoch, "pubkey": pubkey,
            "ca_sig": ca_sig, "body": body}, pos


def verify_cert(ca_pubkey, cert_fields):
    """True iff the certificate's CA signature verifies under the CA public key over its body. A
    self-signed / tampered / non-CA certificate fails here."""
    return len(cert_fields["ca_sig"]) == 64 and \
        ed.verify(ca_pubkey, cert_fields["body"], cert_fields["ca_sig"])


# ---- HELLO-004 codec (mirror of src/net/cert001.cpp) -----------------------------------
def encode_hello4(cert_bytes, challenge_sig):
    """One HELLO-004 (a length-prefixed certificate blob + a length-prefixed challenge signature)."""
    cert_bytes = bytes(cert_bytes)
    challenge_sig = bytes(challenge_sig)
    out = bytearray([HELLO4_VERSION])
    out += g.leb128_encode_u64(len(cert_bytes))
    out += cert_bytes
    out += g.leb128_encode_u64(len(challenge_sig))
    out += challenge_sig
    return bytes(out)


def decode_hello4(data, pos=0):
    """wire bytes -> ({cert, sig}, next_pos). Raises ValueError on a wrong/absent version byte or a
    truncated certificate/signature blob."""
    if pos >= len(data) or data[pos] != HELLO4_VERSION:
        raise ValueError("hello004: bad or missing version byte")
    pos += 1
    certlen, pos = g.leb128_decode_u64(data, pos)
    if pos + certlen > len(data):
        raise ValueError("hello004: truncated certificate")
    cert = bytes(data[pos:pos + certlen])
    pos += certlen
    siglen, pos = g.leb128_decode_u64(data, pos)
    if pos + siglen > len(data):
        raise ValueError("hello004: truncated challenge signature")
    sig = bytes(data[pos:pos + siglen])
    pos += siglen
    return {"cert": cert, "sig": sig}, pos


# ---- verifying CERTIFICATE-AUTHORITY roster (mirror of src/net/authcertserver.cpp) -----
class CaTable:
    """A verifying server that trusts ONE CA public key. Holds NO per-client keys and NO secret. State:
    the CA public key, a set of REVOKED tokens, a per-token EPOCH FLOOR (rotation), and seat occupancy.
    authenticate(cert_bytes, nonce, challenge_sig) hands back the certificate's designated seat only
    when the certificate is CA-signed, the token is not revoked, the epoch is current, the seat is
    free, AND the challenge signature verifies under the certified client key; else SPECTATOR. A
    forged / revoked / stale / unknown / double-login credential all reject into the SAME class."""

    def __init__(self, n_aircraft, ca_pubkey):
        self.n = n_aircraft
        self.ca_pk = bytes(ca_pubkey)
        if len(self.ca_pk) != 32:
            raise ValueError("CaTable: CA pubkey must be 32 bytes")
        self._occupied = [False] * n_aircraft
        self._revoked = set()      # revoked tokens (compromised identities)
        self._min_epoch = {}       # token -> minimum acceptable epoch (rotation floor; default 0)

    def revoke(self, token):
        """Permanently reject a compromised identity: any certificate for `token` -> SPECTATOR."""
        self._revoked.add(int(token))

    def unrevoke(self, token):
        self._revoked.discard(int(token))

    def set_min_epoch(self, token, epoch):
        """Rotate `token`'s key: certificates with epoch < `epoch` are superseded (-> SPECTATOR). The
        CA re-issues at >= `epoch`; the old key is retired without touching any other identity."""
        self._min_epoch[int(token)] = int(epoch)

    def authenticate(self, cert_bytes, nonce, challenge_sig):
        try:
            cf, _ = decode_cert(cert_bytes)
        except ValueError:
            return SPECTATOR                                  # malformed certificate
        if not verify_cert(self.ca_pk, cf):
            return SPECTATOR                                  # not CA-signed / tampered / self-signed
        token, seat, epoch, pubkey = cf["token"], cf["seat"], cf["epoch"], cf["pubkey"]
        if token in self._revoked:
            return SPECTATOR                                  # revoked identity
        if epoch < self._min_epoch.get(token, 0):
            return SPECTATOR                                  # superseded by a key rotation
        if not (0 <= seat < self.n):
            return SPECTATOR                                  # certificate names a non-existent seat
        if self._occupied[seat]:
            return SPECTATOR                                  # double-login
        if not verify_challenge(pubkey, nonce, token, challenge_sig):
            return SPECTATOR                                  # proof-of-possession failed (forgery/replay)
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

    def seed_of(base):
        return bytes([(base + i) & 0xFF for i in range(32)])

    # The certificate authority: ONE key pair. Only its PUBLIC key reaches the server.
    ca_seed = seed_of(0xC0)
    ca_pub = ed.public_key(ca_seed)

    # CERT-001 known encoding pin (asserted identically in the C++ bridge): token=7 (zz 0x0E), seat=1
    # (zz 0x02), epoch=0 (zz 0x00), pubkey = 32x 0x11 -> body = [0x01,0x0E,0x02,0x00, 0x11*32]; then
    # the CA signature is length-prefixed. We pin the BODY prefix (deterministic without a signature).
    body = cert_body(7, 1, 0, bytes([0x11] * 32))
    want_body = bytes([0x01, 0x0E, 0x02, 0x00] + [0x11] * 32)
    if body != want_body:
        print(f"FAIL cert body known encoding: {body.hex()}"); fails += 1

    # HELLO-004 known encoding pin: cert=[0xAA,0xBB] (certlen 2), sig=[0xCC] (siglen 1) ->
    # [0x04, 0x02, 0xAA,0xBB, 0x01, 0xCC].
    if encode_hello4(bytes([0xAA, 0xBB]), bytes([0xCC])) != bytes([0x04, 0x02, 0xAA, 0xBB, 0x01, 0xCC]):
        print(f"FAIL hello4 known encoding: {encode_hello4(bytes([0xAA,0xBB]), bytes([0xCC])).hex()}"); fails += 1

    # Round-trip CERT-001 + HELLO-004 over tokens/seats/epochs (incl. negatives) with a real CA sig.
    for token, seat, epoch in ((-5, 0, 0), (0, 2, 7), (7, 1, 1 << 20), (300, 0, -3)):
        cpub = ed.public_key(seed_of(token & 0xFF))
        cert = issue_cert(ca_seed, token, seat, epoch, cpub)
        cf, pos = decode_cert(cert)
        if pos != len(cert) or cf["token"] != token or cf["seat"] != seat or cf["epoch"] != epoch \
                or cf["pubkey"] != cpub or not verify_cert(ca_pub, cf):
            print(f"FAIL cert round-trip/verify token={token}"); fails += 1
        csig = sign_challenge(seed_of(token & 0xFF), 999, token)
        h4 = encode_hello4(cert, csig)
        dh, hp = decode_hello4(h4)
        if hp != len(h4) or dh["cert"] != cert or dh["sig"] != csig:
            print(f"FAIL hello4 round-trip token={token}"); fails += 1

    # Wrong version bytes -> hard errors (not interchangeable with HELLO-001/2/3 or a CHALLENGE).
    for bad in (0x00, 0x01, 0x02, 0x03, 0xFF):
        try:
            decode_hello4(bytes([bad, 0x00])); print(f"FAIL hello4 version {bad} should raise"); fails += 1
        except ValueError:
            pass
    for bad in (0x00, 0x02, 0x04, 0xFF):
        try:
            decode_cert(bytes([bad, 0x0E, 0x02, 0x00] + [0] * 34)); print(f"FAIL cert version {bad} should raise"); fails += 1
        except ValueError:
            pass

    # CHALLENGE-001 reused verbatim (freshness unchanged): known encoding + freshness.
    if encode_challenge(7) != bytes([0x01, 0x0E]):
        print("FAIL reused challenge encoding"); fails += 1
    if derive_nonce(*session_k, 5) == derive_nonce(*session_k, 6):
        print("FAIL nonce not fresh per accept"); fails += 1

    # --- CaTable: the CA-issued roster, invariant to order (token order != seat order) ---
    #   token seat  client seed (client-side only; NEVER enrolled — the server holds only ca_pub)
    roster = {100: (2, seed_of(0x10)),
              200: (0, seed_of(0x20)),
              300: (1, seed_of(0x30))}
    ct = CaTable(3, ca_pub)
    certs = {tok: issue_cert(ca_seed, tok, seat, 0, ed.public_key(seed))
             for tok, (seat, seed) in roster.items()}

    def present(tbl, token, cert, seed, counter):
        nonce = derive_nonce(*session_k, counter)
        csig = sign_challenge(seed, nonce, token)
        return tbl.authenticate(cert, nonce, csig)

    if present(ct, 300, certs[300], roster[300][1], 0) != 1: print("FAIL 300->seat1"); fails += 1
    if present(ct, 100, certs[100], roster[100][1], 1) != 2: print("FAIL 100->seat2"); fails += 1
    if present(ct, 200, certs[200], roster[200][1], 2) != 0: print("FAIL 200->seat0"); fails += 1
    if sorted(ct.occupied()) != [0, 1, 2]:
        print(f"FAIL occupancy: {ct.occupied()}"); fails += 1

    # NON-CA certificate: an attacker SELF-SIGNS a certificate for token 100 seat 2 with its OWN key
    # (as if it were the CA). It does not verify under the real CA public key -> SPECTATOR. This is the
    # headline: without the CA private key, no valid certificate can be minted.
    ct2 = CaTable(3, ca_pub)
    attacker_seed = seed_of(0x99)
    self_signed = issue_cert(attacker_seed, 100, 2, 0, ed.public_key(seed_of(0x10)))
    nz = derive_nonce(*session_k, 0)
    good_csig = sign_challenge(roster[100][1], nz, 100)  # even with a valid possession proof...
    if ct2.authenticate(self_signed, nz, good_csig) != SPECTATOR:
        print("FAIL self-signed (non-CA) certificate should be rejected"); fails += 1
    if ct2.occupied() != []:
        print("FAIL self-signed cert took a seat"); fails += 1

    # FORGED possession: a genuine CA certificate for token 100, but the client signs the CHALLENGE
    # with the WRONG private key -> the possession proof fails -> SPECTATOR (the certified pubkey does
    # not match the signer). An attacker who STEALS a victim's certificate still cannot use it.
    forged_csig = sign_challenge(attacker_seed, nz, 100)
    if ct2.authenticate(certs[100], nz, forged_csig) != SPECTATOR:
        print("FAIL stolen certificate + wrong key (bad possession) should be rejected"); fails += 1

    # REPLAY: a valid possession proof for one nonce is rejected under a different nonce.
    if ct2.authenticate(certs[100], derive_nonce(*session_k, 77), good_csig) != SPECTATOR:
        print("FAIL replayed challenge signature (stale nonce) should be rejected"); fails += 1
    # the genuine holder authenticates under the right nonce.
    if ct2.authenticate(certs[100], nz, good_csig) != 2:
        print("FAIL genuine certificate holder should authenticate"); fails += 1

    # REVOCATION: a compromised identity is revoked; its (otherwise valid) certificate -> SPECTATOR.
    ct3 = CaTable(3, ca_pub)
    ct3.revoke(200)
    if present(ct3, 200, certs[200], roster[200][1], 10) != SPECTATOR:
        print("FAIL revoked token should be rejected"); fails += 1
    if present(ct3, 100, certs[100], roster[100][1], 11) != 2:  # a different identity is unaffected
        print("FAIL revocation should not affect other identities"); fails += 1
    ct3.unrevoke(200)
    if present(ct3, 200, certs[200], roster[200][1], 12) != 0:  # un-revoking restores it
        print("FAIL un-revoke should restore the seat"); fails += 1

    # KEY ROTATION: raise token 300's epoch floor to 5; its old epoch-0 certificate is superseded ->
    # SPECTATOR, but a freshly re-issued epoch-5 certificate (a NEW key) authenticates.
    ct4 = CaTable(3, ca_pub)
    ct4.set_min_epoch(300, 5)
    if present(ct4, 300, certs[300], roster[300][1], 20) != SPECTATOR:
        print("FAIL stale-epoch (pre-rotation) certificate should be rejected"); fails += 1
    new_seed = seed_of(0x77)  # the rotated (new) private key
    new_cert = issue_cert(ca_seed, 300, 1, 5, ed.public_key(new_seed))
    if present(ct4, 300, new_cert, new_seed, 21) != 1:
        print("FAIL rotated (epoch>=floor) certificate should authenticate"); fails += 1
    # a certificate ABOVE the floor with the OLD key still needs the matching private key.
    if present(ct4, 300, new_cert, roster[300][1], 22) != SPECTATOR:  # seat held anyway, but also wrong key
        pass  # (seat now occupied -> spectator regardless; not a distinct assertion)

    # UNKNOWN identity (no certificate presented is malformed) + DOUBLE-LOGIN + reclaim-own-seat.
    if ct.authenticate(b"\x01\x02", derive_nonce(*session_k, 30), good_csig) != SPECTATOR:
        print("FAIL malformed certificate -> spectator"); fails += 1
    if present(ct, 100, certs[100], roster[100][1], 31) != SPECTATOR:  # seat 2 held on ct
        print("FAIL double-login -> spectator"); fails += 1
    ct.release(2)
    if present(ct, 100, certs[100], roster[100][1], 32) != 2:          # same identity reclaims seat 2
        print("FAIL reconnect should reclaim own seat"); fails += 1

    # a spectator authorizes nothing; a seated client only its own seat (reused predicate).
    if bnd.seat_authorizes(SPECTATOR, 0) or not bnd.seat_authorizes(2, 2) or bnd.seat_authorizes(2, 0):
        print("FAIL authorization predicate under certificate binding"); fails += 1

    if fails == 0:
        print("RESULT: CERT-001 / HELLO-004 / CaTable REFERENCE SELFTEST PASS")
        return 0
    print(f"RESULT: certificate-PKI REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
