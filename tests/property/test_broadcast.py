"""Dynamic-membership properties for the netcode layer-9 single-thread select() broadcast server.

The byte-exact end-to-end claim (a full client reconstructs the sealed SESSION-SK-001 digest, a
late joiner receives exactly the frame suffix from its join point, a leaver receives a clean prefix)
is proven over REAL 127.0.0.1 sockets by the determinism bridge src/net/netdyn_test_main.cpp
(ctest netdyn_bridge). Here we prove the underlying MEMBERSHIP model — which frames each client
receives as a pure function of when it was connected — is self-consistent and composes with the
layer-7 framing codec:

  * a client present for frame-index window [join, leave) receives EXACTLY those frames, contiguous;
  * full (join=0, never leaves)  -> the whole stream;
  * late (join=j>0, never leaves) -> exactly the suffix frames[j:];
  * leaver (join=0, leaves at L)  -> exactly the prefix frames[:L];
  * reassembling any client's delivered frames (each shipped atomically, length-prefixed) through
    an arbitrary TCP chunking yields byte-identical payloads (fan-out adds no per-client info).

This mirrors netbcast::broadcast_select: a joiner accepted before frame fi receives fi onward; a
leaver dropped before frame fi does not receive fi.
"""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import framing_ref as fr


def broadcast_delivery(n_frames, members):
    """Reference layer-9 delivery: client c (present for fi in [join, leave)) receives frame fi.
    Returns {client: [frame indices received, ascending]} — a pure function of the membership."""
    out = {c: [] for c in members}
    for fi in range(n_frames):
        for c, (join, leave) in members.items():
            if join <= fi < leave:
                out[c].append(fi)
    return out


def broadcast_delivery_catchup(n_frames, members):
    """Reference layer-10 delivery WITH catch-up: a joiner accepted at frame `join` is first
    replayed the missed prefix frames[0:join], then streamed the live window [join, leave). So a
    client that stays to the end (leave==n_frames) receives the WHOLE frames[0:] regardless of when
    it joined — the late joiner sees the whole fight. Mirrors netbcast::broadcast_select(catchup=1):
    accept_pending replays frames[0:fi] to a new client, then it enters the live broadcast at fi."""
    out = {}
    for c, (join, leave) in members.items():
        prefix = list(range(0, join))            # catch-up replay
        live = list(range(join, leave))          # live broadcast window
        out[c] = prefix + live                   # == range(0, leave)
    return out


N = st.integers(min_value=0, max_value=40)


@given(N, st.data())
def test_window_is_exact_contiguous_range(n_frames, data):
    # Each client receives EXACTLY the frames in its [join, leave) window — contiguous, no gaps.
    members = {}
    for c in range(data.draw(st.integers(min_value=1, max_value=4))):
        join = data.draw(st.integers(min_value=0, max_value=n_frames))
        leave = data.draw(st.integers(min_value=join, max_value=n_frames))
        members[c] = (join, leave)
    got = broadcast_delivery(n_frames, members)
    for c, (join, leave) in members.items():
        assert got[c] == list(range(join, leave))


@given(N)
def test_full_client_gets_whole_stream(n_frames):
    got = broadcast_delivery(n_frames, {"full": (0, n_frames)})
    assert got["full"] == list(range(n_frames))


@given(N, st.data())
def test_late_join_is_the_suffix(n_frames, data):
    j = data.draw(st.integers(min_value=0, max_value=n_frames))
    got = broadcast_delivery(n_frames, {"late": (j, n_frames)})
    assert got["late"] == list(range(j, n_frames))  # exactly frames[j:], no earlier frame leaks


@given(N, st.data())
def test_leaver_is_the_prefix(n_frames, data):
    leave = data.draw(st.integers(min_value=0, max_value=n_frames))
    got = broadcast_delivery(n_frames, {"leaver": (0, leave)})
    assert got["leaver"] == list(range(0, leave))  # exactly frames[:leave], then gone


@given(N, st.data())
def test_leave_does_not_disturb_a_coresident_full_client(n_frames, data):
    # A leaver departing mid-stream must not change what a co-resident full client receives.
    leave = data.draw(st.integers(min_value=0, max_value=n_frames))
    members = {"full": (0, n_frames), "leaver": (0, leave)}
    got = broadcast_delivery(n_frames, members)
    assert got["full"] == list(range(n_frames))            # full is unaffected
    assert got["leaver"] == list(range(0, leave))


@given(st.lists(st.binary(min_size=0, max_size=80), min_size=1, max_size=12),
       st.integers(min_value=0, max_value=11),
       st.integers(min_value=1, max_value=7))
def test_delivered_suffix_reassembles_byte_exact(payloads, join, step):
    # Compose the membership model with the layer-7 framing codec: the frames a late joiner receives
    # (frames[join:], each length-prefixed and shipped atomically) reassemble — under ANY TCP
    # chunking — to exactly those payloads. This is the pure-codec form of what netdyn_bridge proves
    # over real sockets.
    join = min(join, len(payloads))
    suffix = [bytes(p) for p in payloads[join:]]
    stream = fr.encode_stream(suffix)
    chunks = [stream[i:i + step] for i in range(0, len(stream), step)]
    assert fr.reassemble(chunks) == suffix


# --- layer 10: late-join CATCH-UP -----------------------------------------------------------------
@given(N, st.data())
def test_catchup_joiner_gets_the_whole_stream(n_frames, data):
    # With catch-up, a joiner accepted at any frame j (and staying to the end) receives the WHOLE
    # frames[0:] — prefix replay frames[0:j] then live frames[j:] — so it CAN reconstruct the whole
    # fight, closing the layer-9 gap. This holds for every join point, incl. an early client (j=0).
    j = data.draw(st.integers(min_value=0, max_value=n_frames))
    got = broadcast_delivery_catchup(n_frames, {"catchup": (j, n_frames)})
    assert got["catchup"] == list(range(n_frames))  # identical to a client present from frame 0
    # ...and byte-identical to what a full (join=0) client received under the plain layer-9 model.
    full = broadcast_delivery(n_frames, {"full": (0, n_frames)})
    assert got["catchup"] == full["full"]


@given(st.lists(st.binary(min_size=0, max_size=80), min_size=1, max_size=12),
       st.integers(min_value=0, max_value=11),
       st.integers(min_value=1, max_value=7))
def test_catchup_stream_reassembles_to_all_frames(payloads, join, step):
    # Compose the catch-up model with the layer-7 framing codec: the bytes a catch-up joiner receives
    # are encode_stream(frames[0:join]) [replay] ++ encode_stream(frames[join:]) [live] — and under
    # ANY TCP chunking that reassembles to ALL payloads (== what an early client got). This is the
    # pure-codec form of what netcatchup_bridge proves over real sockets: replay + live == whole.
    all_payloads = [bytes(p) for p in payloads]
    join = min(join, len(all_payloads))
    stream = fr.encode_stream(all_payloads[:join]) + fr.encode_stream(all_payloads[join:])
    assert stream == fr.encode_stream(all_payloads)  # concatenation is exactly the whole stream
    chunks = [stream[i:i + step] for i in range(0, len(stream), step)]
    assert fr.reassemble(chunks) == all_payloads


# --- layer 11: ASYNC OUTPUT (per-client userspace send buffers) ------------------------------------
def sendbuffer_deliver(frames, accepts):
    """Reference layer-11 send-buffer model — mirrors netbcast::{enqueue_bytes,flush_client}: each
    frame's encoded bytes are APPENDED to a userspace queue, and the kernel accepts an arbitrary
    (OS-timing-chosen) byte count per flush attempt — 0 == EWOULDBLOCK, a full-buffer no-op. The
    delivered bytes are whatever the acceptance pattern has let through, then one final drain (the
    layer's bounded drain phase) emits the remainder. Returns the delivered byte stream."""
    queue = b""
    delivered = b""
    accepts = list(accepts)
    for f in frames:
        queue += fr.encode_stream([f])          # enqueue_bytes: append the length-prefixed frame
        take = min(accepts.pop(0), len(queue)) if accepts else 0
        delivered += queue[:take]               # flush_client: kernel accepted `take` bytes
        queue = queue[take:]
    return delivered + queue                    # drain phase: the pending tail flushes last


@given(st.lists(st.binary(min_size=0, max_size=60), min_size=1, max_size=10), st.data())
def test_sendbuffer_delivery_is_chunking_invariant(frames, data):
    # Whatever per-flush byte counts the kernel accepts (including 0 == EWOULDBLOCK stalls — the
    # slow-client case), the DELIVERED byte stream after the drain is exactly encode_stream(frames):
    # buffering changes WHEN bytes move, never WHICH bytes move or their order. This is the pure
    # form of the netasync_bridge volume-leg claim (a never-reading client still gets the exact
    # stream once it drains).
    frames = [bytes(f) for f in frames]
    accepts = [data.draw(st.integers(min_value=0, max_value=200)) for _ in frames]
    assert sendbuffer_deliver(frames, accepts) == fr.encode_stream(frames)
    # Degenerate corners pin the invariant: a never-accepting kernel (all EWOULDBLOCK — everything
    # rides the drain) and an always-greedy one (nothing ever queues) deliver the same bytes.
    assert sendbuffer_deliver(frames, [0] * len(frames)) == fr.encode_stream(frames)
    assert sendbuffer_deliver(frames, [10**9] * len(frames)) == fr.encode_stream(frames)


@given(st.lists(st.binary(min_size=0, max_size=60), min_size=1, max_size=10),
       st.integers(min_value=1, max_value=7), st.data())
def test_sendbuffer_composes_with_framing(frames, step, data):
    # Compose the send-buffer model with the layer-7 codec: enqueue -> arbitrary partial flushes ->
    # arbitrary TCP re-chunking -> StreamReassembler yields the original frames byte-identically.
    # The full layer-11 receive path (write buffering + wire chunking) is the identity on frames.
    frames = [bytes(f) for f in frames]
    accepts = [data.draw(st.integers(min_value=0, max_value=200)) for _ in frames]
    stream = sendbuffer_deliver(frames, accepts)
    chunks = [stream[i:i + step] for i in range(0, len(stream), step)]
    assert fr.reassemble(chunks) == frames


# --- layer 12: send-buffer BYTE-CAP + drop-slowest -------------------------------------------------
def sendbuffer_deliver_capped(frames, accepts, cap):
    """Reference layer-12 capped send-buffer model — mirrors broadcast_async's enqueue + over_cap
    order exactly: each frame is enqueued and opportunistically flushed (the kernel accepts an
    arbitrary byte count; 0 == EWOULDBLOCK), THEN a pending backlog exceeding cap (cap>0) drops the
    client — delivery stops at the bytes the kernel had accepted (the pending tail is discarded
    whole; there is no drain for a capped client). cap==0 disables the policy (layer-11 behavior).
    Returns (delivered byte stream, capped?)."""
    queue = b""
    delivered = b""
    accepts = list(accepts)
    for f in frames:
        queue += fr.encode_stream([f])          # enqueue_bytes: append the length-prefixed frame
        take = min(accepts.pop(0), len(queue)) if accepts else 0
        delivered += queue[:take]               # flush_client: kernel accepted `take` bytes
        queue = queue[take:]
        if cap and len(queue) > cap:            # over_cap: backlog beyond the drop threshold
            return delivered, True
    return delivered + queue, False             # survivor: the drain phase flushes the tail


@given(st.lists(st.binary(min_size=0, max_size=60), min_size=1, max_size=10),
       st.integers(min_value=0, max_value=300), st.data())
def test_capped_delivery_is_a_byte_prefix_and_cap0_is_layer11(frames, cap, data):
    # For ANY kernel-acceptance pattern and ANY cap, a capped client's delivered bytes are a clean
    # byte-PREFIX of encode_stream(frames) — the cap discards the pending tail whole, it never
    # reorders, corrupts, or skips — and an UNcapped client's delivery is the whole stream, exactly
    # the layer-11 model (the cap decides only WHO is dropped, never WHICH bytes flow). cap=0
    # disables the policy bit-for-bit. This is the pure form of the netcap_bridge claims (FAST
    # byte-identical, SLOW a strict byte-prefix).
    frames = [bytes(f) for f in frames]
    accepts = [data.draw(st.integers(min_value=0, max_value=200)) for _ in frames]
    whole = fr.encode_stream(frames)
    delivered, capped = sendbuffer_deliver_capped(frames, accepts, cap)
    assert whole.startswith(delivered)                       # always a byte-prefix
    if not capped:
        assert delivered == whole                            # a survivor gets everything...
        assert delivered == sendbuffer_deliver(frames, accepts)  # ...== the layer-11 model
    uncapped, dropped = sendbuffer_deliver_capped(frames, accepts, 0)
    assert not dropped and uncapped == sendbuffer_deliver(frames, accepts)


@given(st.lists(st.binary(min_size=0, max_size=60), min_size=1, max_size=10),
       st.integers(min_value=0, max_value=300))
def test_cap_never_bites_a_client_that_keeps_up(frames, cap):
    # Healthy-client immunity: a client whose kernel always accepts everything offered (a reader
    # that keeps up — its backlog returns to zero on every flush) is NEVER dropped by ANY cap, and
    # receives the whole stream. The policy sheds only clients that are actually behind — this is
    # the pure form of the netcap_bridge leg-2 claim (generous cap, capped == 0, sealed digest).
    frames = [bytes(f) for f in frames]
    greedy = [10**9] * len(frames)
    delivered, capped = sendbuffer_deliver_capped(frames, greedy, cap)
    assert not capped
    assert delivered == fr.encode_stream(frames)


# --- layer 13: open-ended LIVE frame SOURCE ---------------------------------------------------------
def live_deliver(source_frames, accepts):
    """Reference layer-13 live-source model — mirrors netbcast::broadcast_live: the loop PULLS one
    frame at a time from a source whose length it never knows (a Python iterator here; the loop
    stops only when the source is exhausted), enqueues it through the SAME send-buffer model as
    layer 11, and retains a HISTORY of what it has produced (the catch-up store). Returns
    (delivered byte stream after the drain, history)."""
    queue = b""
    delivered = b""
    history = []
    accepts = list(accepts)
    it = iter(source_frames)                    # the loop has no len(); only StopIteration ends it
    while True:
        try:
            f = next(it)
        except StopIteration:
            break
        history.append(f)                       # retained AS PRODUCED (catchup=true posture)
        queue += fr.encode_stream([f])          # enqueue_bytes: append the length-prefixed frame
        take = min(accepts.pop(0), len(queue)) if accepts else 0
        delivered += queue[:take]               # flush_client: kernel accepted `take` bytes
        queue = queue[take:]
    return delivered + queue, history           # drain phase: the pending tail flushes last


@given(st.lists(st.binary(min_size=0, max_size=60), min_size=1, max_size=10), st.data())
def test_live_source_delivery_equals_batch(frames, data):
    # A stream PULLED one frame at a time from a source of unknown length delivers — under ANY
    # kernel-acceptance pattern — exactly the bytes the batch layer-11 path delivers for the same
    # frames: producing incrementally adds no bytes, no reordering, no truncation. This is the pure
    # form of the netlive_bridge leg-1 claim (live-stepped kernel stream == precomputed stream,
    # sealed digest reconstructed either way).
    frames = [bytes(f) for f in frames]
    accepts = [data.draw(st.integers(min_value=0, max_value=200)) for _ in frames]
    live, _ = live_deliver(frames, accepts)
    assert live == sendbuffer_deliver(frames, accepts)   # == the batch send-buffer model
    assert live == fr.encode_stream(frames)              # == the canonical encoded stream


@given(st.lists(st.binary(min_size=0, max_size=60), min_size=1, max_size=10), st.data())
def test_live_history_replays_a_joiner_the_whole_stream(frames, data):
    # The on-the-fly retained history equals frames[0:j] after j pulls — so a catch-up joiner
    # accepted at ANY production point j receives history (replay) ++ live suffix == the whole
    # stream, byte-identical to an early client, even though the whole stream never existed when
    # it joined. This is the pure form of the netlive_bridge leg-2 claim.
    frames = [bytes(f) for f in frames]
    _, history = live_deliver(frames, [10**9] * len(frames))
    assert history == frames                             # history is exactly what was produced
    j = data.draw(st.integers(min_value=0, max_value=len(frames)))
    replay_then_live = fr.encode_stream(history[:j]) + fr.encode_stream(frames[j:])
    assert replay_then_live == fr.encode_stream(frames)  # joiner at j reconstructs the whole stream


# --- layer 14: bounded/windowed catch-up ------------------------------------------------------------
def live_deliver_windowed(source_frames, accepts, window):
    """Reference layer-14 windowed live-source model — mirrors netbcast::broadcast_live with
    catchup_window=W: identical pull + send-buffer delivery to live_deliver, but the retained
    catch-up history holds only the LAST W produced payloads (the oldest is evicted as each new
    frame lands; 0 = retain all = the layer-13 model). Returns (delivered, history, trimmed)."""
    queue = b""
    delivered = b""
    history = []
    trimmed = 0
    accepts = list(accepts)
    it = iter(source_frames)                    # the loop has no len(); only StopIteration ends it
    while True:
        try:
            f = next(it)
        except StopIteration:
            break
        history.append(f)                       # retained as produced...
        if window and len(history) > window:    # ...bounded to the last `window` payloads
            history.pop(0)
            trimmed += 1
        queue += fr.encode_stream([f])          # enqueue_bytes: append the length-prefixed frame
        take = min(accepts.pop(0), len(queue)) if accepts else 0
        delivered += queue[:take]               # flush_client: kernel accepted `take` bytes
        queue = queue[take:]
    return delivered + queue, history, trimmed  # drain phase: the pending tail flushes last


@given(st.lists(st.binary(min_size=0, max_size=60), min_size=1, max_size=10),
       st.integers(min_value=1, max_value=12), st.data())
def test_windowed_history_is_the_exact_tail_and_a_joiner_gets_a_contiguous_suffix(frames, window,
                                                                                  data):
    # After j pulls a W-window retains EXACTLY frames[max(0,j-W):j] (the newest W, evicting
    # max(0,j-W) — bounded memory on a stream of any length), so a joiner accepted at production
    # point j receives replay(history) ++ live(frames[j:]) == encode_stream(frames[max(0,j-W):]) —
    # a CONTIGUOUS canonical suffix, frame-aligned, no gap, no duplicate. A window still covering
    # the whole stream so far (W >= j) hands the joiner the whole stream — the layer-13 degenerate
    # case. This is the pure form of the netwindow_bridge claims.
    frames = [bytes(f) for f in frames]
    j = data.draw(st.integers(min_value=0, max_value=len(frames)))
    _, history, trimmed = live_deliver_windowed(frames[:j], [10**9] * j, window)
    start = max(0, j - window)
    assert history == frames[start:j]           # the retained window is the exact tail
    assert trimmed == start                     # every older retention was evicted, none skipped
    replay_then_live = fr.encode_stream(history) + fr.encode_stream(frames[j:])
    assert replay_then_live == fr.encode_stream(frames[start:])  # a contiguous canonical suffix
    if window >= j:                             # degenerate: window covers everything produced
        assert replay_then_live == fr.encode_stream(frames)


@given(st.lists(st.binary(min_size=0, max_size=60), min_size=1, max_size=10),
       st.integers(min_value=0, max_value=12), st.data())
def test_window_never_changes_live_delivery_and_window0_is_layer13(frames, window, data):
    # The window bounds ONLY how far back a joiner's replay reaches — a live client's delivered
    # bytes are identical under ANY window (and any kernel-acceptance pattern) to the unwindowed
    # layer-13 model == the canonical encoded stream. window=0 disables eviction bit-for-bit
    # (history == everything produced, trimmed == 0): layer-13 behavior exactly.
    frames = [bytes(f) for f in frames]
    accepts = [data.draw(st.integers(min_value=0, max_value=200)) for _ in frames]
    delivered_w, _, _ = live_deliver_windowed(frames, accepts, window)
    delivered_13, history_13 = live_deliver(frames, accepts)
    assert delivered_w == delivered_13                   # the window never touches live bytes
    assert delivered_w == fr.encode_stream(frames)       # == the canonical encoded stream
    delivered_0, history_0, trimmed_0 = live_deliver_windowed(frames, accepts, 0)
    assert (delivered_0, history_0, trimmed_0) == (delivered_13, history_13, 0)


# --- layer 15a: heartbeat / liveness-timeout LEAVE ------------------------------------------------
def live_deliver_liveness(frames, accepts, liveness):
    """Reference layer-15a liveness-reap model — mirrors netbcast::{flush_client,reap_dead_async}:
    each frame is enqueued and the kernel accepts `accepts[i]` bytes (0 == a stalled/dead receiver),
    THEN (once per produced frame) a client that has made NO receive progress — its buffer is
    non-empty AND no bytes left the kernel since the last check — for more than `liveness`
    consecutive frames is REAPED (its pending tail discarded whole, like a cap drop; delivery stops
    at the kernel-accepted bytes). A frame that empties the buffer OR advances the cumulative sent
    count resets the idle counter, so a client draining ANY bytes is never reaped. liveness==0
    disables the policy (layer-13 behavior: no reap, the tail flushes in the drain). Returns
    (delivered byte stream, reaped?)."""
    queue = b""
    delivered = b""
    sent_total = 0
    last_sent = 0
    idle = 0
    accepts = list(accepts)
    for i, f in enumerate(frames):
        queue += fr.encode_stream([f])          # enqueue_bytes: append the length-prefixed frame
        take = min(accepts[i], len(queue)) if i < len(accepts) else 0
        delivered += queue[:take]               # flush_client: kernel accepted `take` bytes
        queue = queue[take:]
        sent_total += take                       # ...tracked as the receive-progress signal
        if len(queue) == 0 or sent_total > last_sent:   # progress: fully drained or bytes moved
            idle = 0
            last_sent = sent_total
        elif liveness:                           # stalled this frame; count against the deadline
            idle += 1
            if idle > liveness:
                return delivered, True           # reaped: the pending tail is discarded whole
    return delivered + queue, False              # survivor: the drain phase flushes the tail


@given(st.lists(st.binary(min_size=1, max_size=60), min_size=2, max_size=12), st.data())
def test_liveness_reaps_a_dead_client_and_a_prefix_is_delivered(frames, data):
    # A client that STOPS receiving at some frame d (kernel accepts nothing thereafter) is reaped
    # once it has been silent for > liveness frames — its delivered bytes are a clean byte-PREFIX of
    # the encoded stream (the tail is discarded whole, never reordered) and strictly short of it (it
    # died before the end). The pure form of the netheartbeat_bridge reap leg. A generous head start
    # (everything accepted through frame d) makes the pre-death buffer empty, so the deadline starts
    # cleanly at d; the death frame d is drawn to leave > liveness silent frames after it so the
    # deadline provably elapses within the stream.
    frames = [bytes(f) for f in frames]
    n = len(frames)
    liveness = data.draw(st.integers(min_value=1, max_value=n - 1))   # < n: a from-0-dead client
                                                                     # can still exceed the deadline
    d = data.draw(st.integers(min_value=0, max_value=n - 1 - liveness))  # silent = n-d >= liveness+1
    accepts = [10 ** 9] * d + [0] * (n - d)             # alive through d, then dead
    whole = fr.encode_stream(frames)
    delivered, reaped = live_deliver_liveness(frames, accepts, liveness)
    assert reaped                                        # a permanently-silent client IS reaped
    assert whole.startswith(delivered)                  # exactly a byte-prefix (tail discarded)
    assert len(delivered) < len(whole)                  # strictly short — it died mid-stream
    # liveness==0 disables the reap bit-for-bit: the same acceptance pattern delivers everything.
    delivered0, reaped0 = live_deliver_liveness(frames, accepts, 0)
    assert not reaped0 and delivered0 == whole


@given(st.lists(st.binary(min_size=0, max_size=60), min_size=1, max_size=10),
       st.integers(min_value=1, max_value=8))
def test_liveness_never_bites_a_client_that_keeps_up(frames, liveness):
    # Healthy-client immunity (the mirror of test_cap_never_bites_a_client_that_keeps_up): a client
    # whose kernel always accepts everything offered — its backlog returns to zero every frame — is
    # NEVER reaped by ANY liveness deadline, and receives the whole stream. Liveness sheds only a
    # client that has stopped receiving, exactly the netheartbeat_bridge healthy leg.
    frames = [bytes(f) for f in frames]
    greedy = [10 ** 9] * len(frames)
    delivered, reaped = live_deliver_liveness(frames, greedy, liveness)
    assert not reaped
    assert delivered == fr.encode_stream(frames)
