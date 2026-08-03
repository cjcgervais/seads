<#
############################################################################
# DEAD — DO NOT USE. This server has no producer.
#
# It pairs with src/shared/FlightRecorder.luau, whose POST draft was struck
# down by review and rewritten as print-only (recorded in FlightRecorder's
# own header, and independently confirmed by PREFLIGHT-GATE.md S2/S3 and
# SOP-01-PRIMARY-DATA.md's "WHY THIS EXISTS" incident report, 2026-08-02).
# Four independent things also block the path even if that weren't true:
# HttpEnabled=false in every .rbxlx in the EvC2026 tree, nothing ever
# listens on this script's port from the shipped client, this script listens
# on 8790 expecting JSON {session, frames} while the client (when it posted
# at all) sent text/plain CSV, and BirdController.client.luau printed
# "sent N bytes -> server relay" on FireServer's RETURN — i.e. it reported
# success for a send that had already been discarded server-side. Starting
# this server and believing data is landing is the exact defect SOP-01
# exists to prevent.
#
# The only sink SOP-01 trusts is the Studio log itself: print -> CreatorOutput
# -> the log file, read directly. See ../SOP-01-PRIMARY-DATA.md and
# ../TAPE-SCHEMA.md. gate.py's item E1 checks for this banner.
#
# Left in place, not deleted, so the SEADS-side workflow in
# ../legacy-replay/REPLAY_ROADMAP.md and replay_diff.py --help text that
# still mentions this file remain traceable to why it stopped being used.
############################################################################
.SYNOPSIS
    Flight-feel capture server for the Eagles vs Crows golden replay harness.

.DESCRIPTION
    A dependency-free PowerShell 5.1 HttpListener that receives batched flight-frame
    JSON from the in-game FlightRecorder (src/shared/FlightRecorder.luau) and appends
    it to a .jsonl file under mandalark-kernel/captures. One file per recording session
    (keyed by the session id inside the payload); line 1 is the session metadata, every
    subsequent line is one flight frame.

    Prints a live status line per batch: frames received, capture duration, last speed.

    Stop it with Ctrl+C. No external modules, no admin rights (localhost only).

.PARAMETER Port
    TCP port to listen on (default 8790, matching FlightRecorder's default URL).

.PARAMETER CaptureDir
    Where .jsonl files are written (default ..\captures relative to this script).

.EXAMPLE
    .\capture_server.ps1
    Then in Roblox Studio, start the recorder and fly. Ctrl+C here when done.
#>
[CmdletBinding()]
param(
    [int]$Port = 8790,
    [string]$CaptureDir = (Join-Path $PSScriptRoot '..\captures')
)

$ErrorActionPreference = 'Stop'

# Resolve + ensure the capture directory exists.
if (-not (Test-Path $CaptureDir)) {
    New-Item -ItemType Directory -Path $CaptureDir -Force | Out-Null
}
$CaptureDir = (Resolve-Path $CaptureDir).Path

# sessionId -> ordered file/stat record, so multi-batch sessions append to one file.
$sessions = @{}

function New-SafeName([string]$s) {
    if ([string]::IsNullOrWhiteSpace($s)) { return 'session' }
    return ($s -replace '[^\w\-]', '-')
}

$listener = New-Object System.Net.HttpListener
$prefix = "http://localhost:$Port/"
$listener.Prefixes.Add($prefix)

try {
    $listener.Start()
} catch {
    Write-Host "FAILED to start listener on $prefix" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host "If it's an access error, try a different -Port or run: netsh http add urlacl url=$prefix user=$env:USERNAME" -ForegroundColor Yellow
    exit 1
}

Write-Host "Capture server listening on $prefix" -ForegroundColor Green
Write-Host "  Writing captures to: $CaptureDir" -ForegroundColor Gray
Write-Host "  POST flight batches to ${prefix}capture   |   GET ${prefix}health   |   Ctrl+C to stop" -ForegroundColor Gray
Write-Host ""

try {
    while ($listener.IsListening) {
        # GetContext blocks until a request arrives; Ctrl+C interrupts the whole process.
        $context = $listener.GetContext()
        $request = $context.Request
        $response = $context.Response

        try {
            $path = $request.Url.AbsolutePath.TrimEnd('/').ToLower()

            if ($request.HttpMethod -eq 'GET' -and ($path -eq '/health' -or $path -eq '')) {
                $body = '{"ok":true,"sessions":' + $sessions.Count + '}'
                $bytes = [System.Text.Encoding]::UTF8.GetBytes($body)
                $response.ContentType = 'application/json'
                $response.OutputStream.Write($bytes, 0, $bytes.Length)
                $response.StatusCode = 200
                $response.Close()
                continue
            }

            if ($request.HttpMethod -ne 'POST') {
                $response.StatusCode = 405
                $response.Close()
                continue
            }

            # Read the raw JSON body.
            $reader = New-Object System.IO.StreamReader($request.InputStream, $request.ContentEncoding)
            $raw = $reader.ReadToEnd()
            $reader.Close()

            $payload = $null
            try {
                $payload = $raw | ConvertFrom-Json
            } catch {
                $response.StatusCode = 400
                $response.Close()
                Write-Host ("  bad JSON ({0} bytes): {1}" -f $raw.Length, $_.Exception.Message) -ForegroundColor Red
                continue
            }

            $sess = $payload.session
            $frames = $payload.frames
            if ($null -eq $sess -or $null -eq $frames) {
                $response.StatusCode = 422
                $response.Close()
                Write-Host "  payload missing 'session' or 'frames'" -ForegroundColor Red
                continue
            }

            $sid = [string]$sess.id
            if (-not $sessions.ContainsKey($sid)) {
                # First batch for this session: create the file and write the metadata header line.
                $stamp = (Get-Date).ToString('yyyyMMdd-HHmmss')
                $tag = New-SafeName ([string]$sess.versionTag)
                $fileName = "$stamp`_$tag`_$(New-SafeName $sid).jsonl"
                $filePath = Join-Path $CaptureDir $fileName
                $headerLine = ($sess | ConvertTo-Json -Depth 10 -Compress)
                # Wrap the header so the diff tool can tell metadata from frames.
                '{"session":' + $headerLine + '}' | Out-File -FilePath $filePath -Encoding utf8
                $sessions[$sid] = [ordered]@{
                    file = $filePath
                    frames = 0
                    firstT = $null
                    lastT = 0
                    lastSpeed = 0
                }
                Write-Host ("NEW session $sid  ->  $fileName") -ForegroundColor Cyan
            }

            $rec = $sessions[$sid]
            # Append each frame as its own JSONL line.
            $lines = New-Object System.Collections.Generic.List[string]
            foreach ($f in $frames) {
                $lines.Add(($f | ConvertTo-Json -Depth 10 -Compress))
                $rec.frames++
                if ($null -eq $rec.firstT) { $rec.firstT = [double]$f.t }
                $rec.lastT = [double]$f.t
                if ($null -ne $f.speed) { $rec.lastSpeed = [double]$f.speed }
            }
            if ($lines.Count -gt 0) {
                Add-Content -Path $rec.file -Value $lines -Encoding utf8
            }

            $dur = [math]::Round($rec.lastT - ([double]$rec.firstT), 2)
            Write-Host ("  +{0,3} frames | total {1,5} | {2,7:0.00}s | last speed {3,7:0.0}" -f `
                $frames.Count, $rec.frames, $dur, $rec.lastSpeed)

            $ok = [System.Text.Encoding]::UTF8.GetBytes('{"ok":true}')
            $response.ContentType = 'application/json'
            $response.OutputStream.Write($ok, 0, $ok.Length)
            $response.StatusCode = 200
            $response.Close()
        }
        catch {
            Write-Host ("  request error: {0}" -f $_.Exception.Message) -ForegroundColor Red
            try { $response.StatusCode = 500; $response.Close() } catch {}
        }
    }
}
finally {
    Write-Host ""
    Write-Host "Stopping capture server..." -ForegroundColor Yellow
    foreach ($k in $sessions.Keys) {
        $r = $sessions[$k]
        Write-Host ("  session {0}: {1} frames -> {2}" -f $k, $r.frames, (Split-Path $r.file -Leaf)) -ForegroundColor Gray
    }
    $listener.Stop()
    $listener.Close()
}
