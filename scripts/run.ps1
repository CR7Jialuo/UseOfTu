# Run the program (builds first if needed). ASCII only.
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Test-Path 'build/nav.exe')) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($args.Count -eq 0) {
    & '.\build\nav.exe'
} else {
    & '.\build\nav.exe' @args
}
exit $LASTEXITCODE
