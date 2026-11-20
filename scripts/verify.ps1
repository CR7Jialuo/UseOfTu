# Pre-submission verification: build + unit tests + e2e + data checks (ASCII only).
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host '== 1/4 build ==' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'build.ps1')
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 2/4 unit tests ==' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'test.ps1')
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 3/4 e2e ==' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'e2e.ps1')
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 4/4 repository checks ==' -ForegroundColor Cyan
$required = @(
    'README.md', 'Makefile', 'include/nav.h', 'src/main.c', 'src/algo.c',
    'tests/test_main.c', 'data/campus/spots.csv', 'data/campus/roads.csv',
    '.github/workflows/ci.yml'
)
$missing = 0
foreach ($f in $required) {
    if (-not (Test-Path $f)) {
        Write-Host "  missing: $f" -ForegroundColor Red
        $missing++
    }
}
if ($missing -gt 0) { exit 1 }

$git = Join-Path (Split-Path -Parent $root) '.tools\git\cmd\git.exe'
if (Test-Path $git) {
    $status = & $git status --porcelain
    if ($status) {
        Write-Host '  warning: working tree not clean:' -ForegroundColor Yellow
        $status | ForEach-Object { Write-Host "    $_" }
    } else {
        Write-Host '  working tree clean'
    }
}

Write-Host ''
Write-Host 'verify passed.' -ForegroundColor Green
