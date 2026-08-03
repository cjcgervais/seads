# Wiring the Roblox FlightRecorder (Eagles vs Crows)

> ## ⛔ DEAD — DO NOT FOLLOW THESE INSTRUCTIONS
> `FlightRecorder`'s HTTP-POST path was struck down by review and rewritten
> print-only; `capture_server.ps1` (same directory) has no producer and never
> will while that stands. Wiring this in gets you a client that warns once
> and buffers forever, or — worse — a server print that claims success for a
> POST that was discarded. See `capture_server.ps1`'s own DEAD banner and
> `../SOP-01-PRIMARY-DATA.md`. The only sink SOP-01 trusts today is
> `print` → the Studio log, per `../TAPE-SCHEMA.md`. Left here, not deleted,
> as a record of the path that was tried and rejected.

The recorder module is **additive** — the one new file
`D:\EvC2026\src\shared\FlightRecorder.luau`. It requires nothing from
BirdController and changes flight by exactly zero until you start it.

## Before you fly (required)

1. **Enable HttpService.** In Studio: **Game Settings → Security → Allow HTTP
   Requests → ON**. (Roblox blocks HTTP otherwise; the recorder will warn once and
   keep buffering, but nothing will reach the server.)
2. **Start the capture server** (separate PowerShell window, leave it open):
   ```
   powershell -ExecutionPolicy Bypass -File D:\mandalark-kernel\harness\capture_server.ps1
   ```

## Wire it in — the lines to paste

Roblox only executes `HttpService:PostAsync` on the **server**, and the flown
bird's position/velocity/nose/bank are replicated to the server, so the reliable
path is a **server Script**. Add a Script (e.g. in `ServerScriptService`, or ask
the other EvC session to drop these lines into an existing server script):

```lua
local FlightRecorder = require(game:GetService("ReplicatedStorage"):WaitForChild("FlightRecorder"))
FlightRecorder.start({ versionTag = "v5-rungE" })   -- POSTs to http://localhost:8790/capture
-- ...fly your test flight...
-- FlightRecorder.stop()   -- optional; or just Ctrl+C the capture server when done
```

That's it. The recorder finds the possessed bird, samples it every Heartbeat, and
ships batches of 60 frames to the server. It captures: timestamp, position,
velocity, nose direction, bank angle, an angle-of-attack proxy, speed, and an
energy proxy.

## Optional — richer capture (raw mouse + cascade state)

Two things live only on the **client** / inside BirdController and are NOT
readable from the server: the **raw mouse-aimer delta** and the **cascade state**
(`push_gate` armed/committed, aim world-vector). The recorder supports them via an
optional feed, but wiring it means touching BirdController — which belongs to the
other EvC session. If you want them, ask that session to add ONE line at the end
of its per-frame step, e.g.:

```lua
FlightRecorder.feed({ aim = aimTargetDir, pushGateArmed = <bool>, pushGateCommitted = <bool> })
```

Whatever is fed rides along under each frame's `kernel` field, and if `aim` is a
`Vector3` the recorder also logs the headline **nose-vs-aim error angle**. Absent
the feed, the diff tool falls back to nose-vs-velocity + trajectory metrics —
still a useful golden, just without the aim-error and push-gate columns.

> Note: if you instead start the recorder from a **client LocalScript**, it will
> also read the raw mouse delta on its own (`UserInputService:GetMouseDelta()`) —
> but the client POST is blocked by Roblox, so it will warn once and buffer
> without shipping. Server-side is the path that actually lands data.
