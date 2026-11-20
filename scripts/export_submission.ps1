# Package the submission: git archive + built exe + final docs -> dist zip (ASCII only).
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$git = Join-Path (Split-Path -Parent $root) '.tools\git\cmd\git.exe'
if (-not (Test-Path $git)) {
    $cmd = Get-Command git -ErrorAction SilentlyContinue
    if (-not $cmd) { Write-Host 'ERROR: git not found' -ForegroundColor Red; exit 1 }
    $git = $cmd.Source
}

$name = 'campus-nav'
$stage = 'dist\stage'
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Write-Host 'exporting tracked files via git archive ...'
$tar = 'dist\stage.tar'
& $git archive --format=tar --output $tar HEAD
if ($LASTEXITCODE -ne 0) { exit 1 }
& tar -xf $tar -C $stage
if ($LASTEXITCODE -ne 0) { exit 1 }
Remove-Item $tar -Force

Write-Host 'adding built binary ...'
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'bin') | Out-Null
if (Test-Path 'build\nav.exe') {
    Copy-Item 'build\nav.exe' (Join-Path $stage 'bin\nav.exe') -Force
} else {
    Write-Host 'warning: build\nav.exe missing (run scripts\build.ps1)' -ForegroundColor Yellow
}

Write-Host 'adding final documents ...'
$docs = Get-ChildItem 'docs' -File | Where-Object { $_.Extension -in '.docx', '.pdf', '.pptx' }
foreach ($d in $docs) {
    Copy-Item $d.FullName (Join-Path $stage ("docs\" + $d.Name)) -Force
}

$zip = 'dist\' + $name + '-submit.zip'
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Host ('package: ' + $zip) -ForegroundColor Green

Write-Host 'contents:'
$entry = Get-ChildItem $stage -Recurse -File | Measure-Object
Write-Host ("  files: " + $entry.Count)
Get-ChildItem $stage | Select-Object Name | ForEach-Object { Write-Host ("  - " + $_.Name) }
