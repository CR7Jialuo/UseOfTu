# Remove Mark-of-the-Web from this folder (fixes SmartScreen "Windows protected your PC").
# ASCII only.
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$root = Split-Path -Parent $PSScriptRoot
Write-Host "unblocking: $root"
$n = 0
Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
    try {
        Unblock-File -LiteralPath $_.FullName -ErrorAction SilentlyContinue
        $n++
    } catch {
    }
}
Write-Host "done: $n files checked"
Write-Host "now double-click build\navgui.exe or run_gui.cmd"
