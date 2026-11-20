# Compile and run the automated test suite (ASCII only).
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Find-Compiler {
    if ($env:CC) { return $env:CC }
    $cmd = Get-Command gcc -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $portable = Join-Path (Split-Path -Parent $root) '.tools\w64devkit\w64devkit\bin\gcc.exe'
    if (Test-Path $portable) { return $portable }
    return $null
}

$gcc = Find-Compiler
if (-not $gcc) {
    Write-Host 'ERROR: gcc not found. Install MinGW-w64 or set CC.' -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path 'build' | Out-Null
# Some environments have a non-writable system temp dir; keep gcc's temp files local.
New-Item -ItemType Directory -Force -Path 'build\tmp' | Out-Null
$env:TMP = (Resolve-Path 'build\tmp').Path
$env:TEMP = $env:TMP

$common = @(
    '-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
    '-finput-charset=UTF-8', '-fexec-charset=UTF-8', '-Iinclude'
)
$core = @(
    'src/campus.c', 'src/graph.c', 'src/algo.c', 'src/app.c', 'src/ui.c',
    'src/platform.c', 'src/crc32.c', 'src/bench.c', 'src/default_campus.c'
)

Write-Host "compiler: $gcc" -ForegroundColor Cyan
& $gcc @common @core 'tests/test_main.c' -o 'build/tests.exe' -static -lm
if ($LASTEXITCODE -ne 0) { throw "test build failed, exit $LASTEXITCODE" }

& '.\build\tests.exe'
if ($LASTEXITCODE -ne 0) { throw "tests failed, exit $LASTEXITCODE" }
Write-Host 'all tests passed.' -ForegroundColor Green
