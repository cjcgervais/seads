# Nightly backup of the EXTERNAL DRIVE (D:, "Expansion") to Google Drive (chad@terraesginsight.ca)
# Reinstated 2026-09-15 on Chad's word, as the SENTINEL's nightly job. It extends (does not replace)
# tools/backup_to_gdrive.ps1, which still mirrors D:\mandalark-kernel on its own at 21:30.
#
# Run by Windows Task Scheduler ("Mandalark Drive Sentinel Backup", daily 22:30); safe by hand:
#   powershell -ExecutionPolicy Bypass -File D:\mandalark-kernel\tools\backup_drive_to_gdrive.ps1
#
# What it does, in order:
#   0. SENTINEL LEDGER  -> records the flight_sim2 origin/main tip and every lane tip (read-only
#                          git fetch + for-each-ref in D:\flight_sim2\seads) into tools\backup_logs
#                          and appends a dated row to docs\SENTINEL_LEDGER.md when main moved.
#   1. git bundles      -> full-history bundles (restorable with `git clone <bundle>`) of the repos
#                          that matter, because step 2 EXCLUDES every .git directory (Drive chokes
#                          on hundreds of thousands of loose objects):
#                            D:\flight_sim2\seads      (one .git: seads-feel, seads-recon and every
#                                                      D:\seads_sandboxes\* lane are worktrees of it)
#                            D:\mandalark-kernel       (this repo -- ⚠ its GitHub remote returned
#                                                      "Repository not found" on 2026-09-15, so this
#                                                      bundle + Drive is the ONLY off-site copy)
#                            D:\EvC2026, D:\SEADS_2026, D:\flying_architecture, D:\EvC2026_sandbox_cascade
#   2. rclone sync      -> mirrors D:\ to Drive folder Mandalark_Backups/D_drive with the excludes
#                          below. A sync DELETES on Drive what was deleted on D: -- with
#                          --drive-use-trash the removed copy sits in Drive's trash for 30 days.
#
# Remote "gdrive-tesg" is authorized against chad@terraesginsight.ca with drive.file scope
# (rclone can only see files it created -- it cannot touch the rest of the Drive).
# ⚠ rclone warns that its SHARED Google client_id is being retired during 2026. When it stops,
#   every run fails at step 2 with an auth error. Fix = Chad makes his own client_id
#   (https://rclone.org/drive/#making-your-own-client-id) and runs `rclone config reconnect gdrive-tesg:`.

$ErrorActionPreference = 'Continue'
$rclone   = "C:\Users\Chad\AppData\Local\Microsoft\WinGet\Packages\Rclone.Rclone_Microsoft.Winget.Source_8wekyb3d8bbwe\rclone-v1.74.4-windows-amd64\rclone.exe"
$src      = 'D:\'
$remote   = 'gdrive-tesg:Mandalark_Backups/D_drive'
$home_    = 'D:\mandalark-kernel'
$logDir   = Join-Path $home_ 'tools\backup_logs'
$bundleDir= Join-Path $home_ 'tools\bundles'
$ledger   = Join-Path $home_ 'docs\SENTINEL_LEDGER.md'
foreach ($d in @($logDir, $bundleDir)) { if (-not (Test-Path $d)) { New-Item -ItemType Directory -Path $d | Out-Null } }
$stamp    = Get-Date -Format 'yyyy-MM-dd'
$log      = Join-Path $logDir ("drive_backup_{0}.log" -f $stamp)
function Log($s) { ("{0}  {1}" -f (Get-Date -Format 'HH:mm:ss'), $s) | Out-File $log -Encoding utf8 -Append }

"=== DRIVE BACKUP started $(Get-Date) ===" | Out-File $log -Encoding utf8 -Append
$failed = 0

# ---------- 0. sentinel ledger (read-only against the live tree) ----------
try {
    $seads = 'D:\flight_sim2\seads'
    git -C $seads fetch -q origin 2>&1 | Out-Null
    $main = (git -C $seads rev-parse --short origin/main).Trim()
    $refs = git -C $seads for-each-ref --sort=-committerdate --format='%(refname:short) %(objectname:short) %(committerdate:short) %(subject)' refs/remotes/origin refs/tags
    $refs | Out-File (Join-Path $logDir ("sentinel_refs_{0}.txt" -f $stamp)) -Encoding utf8
    $lastFile = Join-Path $logDir 'sentinel_last_main.txt'
    $last = if (Test-Path $lastFile) { (Get-Content $lastFile -Raw).Trim() } else { '' }
    if ($main -ne $last) {
        $subj = (git -C $seads log -1 --format='%s' origin/main).Trim()
        $row = "| $stamp | ``$main`` | $subj | MOVED (was ``$last``) -- nightly job observation, not audited |"
        if (Test-Path $ledger) { $row | Out-File $ledger -Encoding utf8 -Append }
        $main | Out-File $lastFile -Encoding utf8
        Log "SENTINEL: origin/main moved $last -> $main"
    } else { Log "SENTINEL: origin/main unchanged at $main" }
} catch { Log "SENTINEL ledger step failed: $_"; $failed++ }

# ---------- 1. bundles ----------
$repos = @(
    @{ path='D:\flight_sim2\seads';        name='flight_sim2-seads.bundle' },
    @{ path='D:\mandalark-kernel';         name='mandalark-kernel.bundle' },
    @{ path='D:\EvC2026';                  name='EvC2026.bundle' },
    @{ path='D:\SEADS_2026';               name='SEADS_2026.bundle' },
    @{ path='D:\flying_architecture';      name='flying_architecture.bundle' },
    @{ path='D:\EvC2026_sandbox_cascade';  name='EvC2026_sandbox_cascade.bundle' }
)
foreach ($r in $repos) {
    if (-not (Test-Path (Join-Path $r.path '.git'))) { Log "bundle SKIP (no .git): $($r.path)"; continue }
    $out = Join-Path $bundleDir $r.name
    $tmp = "$out.tmp"
    & git -C $r.path bundle create $tmp --all 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0 -and (Test-Path $tmp)) {
        Move-Item -Force $tmp $out
        Log ("bundle OK   {0}  {1:N1} MB" -f $r.name, ((Get-Item $out).Length/1MB))
    } else { Log "bundle FAIL $($r.name) (git exit $LASTEXITCODE)"; $failed++; if (Test-Path $tmp) { Remove-Item $tmp -Force } }
}

# ---------- 2. mirror the drive ----------
& $rclone sync $src $remote `
    --exclude '$RECYCLE.BIN/**' `
    --exclude 'System Volume Information/**' `
    --exclude '**/.git/**' `
    --exclude '**/.git' `
    --exclude '**/build/**' --exclude '**/build-*/**' --exclude '**/build_*/**' `
    --exclude '**/node_modules/**' `
    --exclude '**/__pycache__/**' --exclude '**/.venv/**' --exclude '**/venv/**' `
    --exclude '**/Library/**' --exclude '**/Temp/**' --exclude '**/obj/**' `
    --exclude 'Unity Editor/**' --exclude 'Unity Hub/**' `
    --exclude '.npm-cache/**' --exclude 'tmp/**' --exclude 'temp/**' `
    --exclude 'mandalark-kernel/tools/backup_logs/**' `
    --drive-use-trash=true `
    --transfers 8 --checkers 16 --drive-chunk-size 64M `
    --fast-list --retries 3 --low-level-retries 10 `
    --stats 5m --stats-one-line `
    --log-file $log --log-level INFO
if ($LASTEXITCODE -ne 0) { Log "rclone sync exit $LASTEXITCODE"; $failed++ } else { Log "rclone sync OK" }

if ($failed -eq 0) { "=== DRIVE BACKUP OK $(Get-Date) ===" | Out-File $log -Encoding utf8 -Append }
else { "=== DRIVE BACKUP FAILED ($failed step(s)) $(Get-Date) ===" | Out-File $log -Encoding utf8 -Append }

# Keep only the last 21 daily logs of each kind
foreach ($pat in @('drive_backup_*.log','sentinel_refs_*.txt')) {
    Get-ChildItem $logDir -Filter $pat | Sort-Object Name -Descending | Select-Object -Skip 21 | Remove-Item -Force -Confirm:$false
}
if ($failed -ne 0) { exit 1 }
