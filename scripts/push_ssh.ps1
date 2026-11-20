# Push to GitHub over SSH 443 with the repo deploy key (ASCII only).
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\push_ssh.ps1
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$tools = Split-Path -Parent $root

$env:GIT_SSH = Join-Path $tools '.tools\gh-ssh\ssh_usefotu.exe'
$env:GIT_SSH_VARIANT = 'ssh'
$git = Join-Path $tools '.tools\git\cmd\git.exe'

if (-not (Test-Path $env:GIT_SSH)) {
    Write-Host 'ERROR: ssh wrapper missing (.tools\gh-ssh\ssh_usefotu.exe)' -ForegroundColor Red
    exit 1
}

& $git remote -v
& $git push origin main
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host 'pushed.' -ForegroundColor Green
