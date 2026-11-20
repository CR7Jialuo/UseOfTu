# Build the Win64 GUI (navgui.exe). ASCII only.
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
New-Item -ItemType Directory -Force -Path 'build\tmp' | Out-Null
$env:TMP = (Resolve-Path 'build\tmp').Path
$env:TEMP = $env:TMP

$common = @(
    '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-finput-charset=UTF-8', '-fexec-charset=UTF-8',
    '-Iinclude', '-I.', '-DUNICODE', '-D_UNICODE'
)
$core = @('src/campus.c', 'src/graph.c', 'src/algo.c', 'src/platform.c', 'src/crc32.c')
$gui = @('gui/gui.c', 'gui/render.c', 'gui/main.c')

Write-Host "compiler: $gcc" -ForegroundColor Cyan
& $gcc @common @core @gui -o 'build/navgui.exe' -mwindows -static `
    -lgdiplus -lgdi32 -luser32 -lole32 -lshell32 -lcomdlg32 -lm
if ($LASTEXITCODE -ne 0) { throw "GUI build failed, exit $LASTEXITCODE" }
Write-Host 'built: build\navgui.exe' -ForegroundColor Green
