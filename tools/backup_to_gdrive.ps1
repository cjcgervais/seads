# Nightly backup of mandalark-kernel to Google Drive (chad@terraesginsight.ca)
# Run by Windows Task Scheduler; safe to run by hand any time:
#   powershell -ExecutionPolicy Bypass -File D:\mandalark-kernel\tools\backup_to_gdrive.ps1
#
# What it does:
#   1. git bundle  -> a single file containing the ENTIRE repo history (restorable with `git clone`)
#   2. rclone sync -> mirrors the working tree + bundle to Drive folder Mandalark_Backups/mandalark-kernel
# Remote "gdrive-tesg" is authorized against chad@terraesginsight.ca with drive.file scope
# (rclone can only see files it created — it cannot touch the rest of the Drive).

$ErrorActionPreference = 'Stop'
$repo    = 'D:\mandalark-kernel'
$rclone  = "C:\Users\Chad\AppData\Local\Microsoft\WinGet\Packages\Rclone.Rclone_Microsoft.Winget.Source_8wekyb3d8bbwe\rclone-v1.74.4-windows-amd64\rclone.exe"
$remote  = 'gdrive-tesg:Mandalark_Backups/mandalark-kernel'
$logDir  = Join-Path $repo 'tools\backup_logs'
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }
$log     = Join-Path $logDir ("backup_{0}.log" -f (Get-Date -Format 'yyyy-MM-dd'))

"=== Backup started $(Get-Date) ===" | Out-File $log -Encoding utf8 -Append

# 1. Full-history bundle (kept out of git via .gitignore on tools/backup_logs and *.bundle)
$bundleDir = Join-Path $repo 'tools\bundles'
if (-not (Test-Path $bundleDir)) { New-Item -ItemType Directory -Path $bundleDir | Out-Null }
$bundle = Join-Path $bundleDir 'mandalark-kernel.bundle'
git -C $repo bundle create $bundle --all 2>&1 | Out-File $log -Encoding utf8 -Append

# 2. Mirror working tree (minus .git internals; the bundle carries full history) to Drive
& $rclone sync $repo $remote `
    --exclude '.git/**' `
    --exclude 'tools/backup_logs/**' `
    --drive-use-trash=true `
    --log-file $log --log-level INFO

if ($LASTEXITCODE -eq 0) {
    "=== Backup OK $(Get-Date) ===" | Out-File $log -Encoding utf8 -Append
} else {
    "=== Backup FAILED (rclone exit $LASTEXITCODE) $(Get-Date) ===" | Out-File $log -Encoding utf8 -Append
    exit 1
}

# Keep only the last 14 daily logs
Get-ChildItem $logDir -Filter 'backup_*.log' | Sort-Object Name -Descending | Select-Object -Skip 14 | Remove-Item -Force -Confirm:$false
