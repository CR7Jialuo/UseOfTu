# Package the submission: tracked files + built exe + final documents -> dist zip (ASCII only).
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

Write-Host 'exporting tracked files (git archive, zip) ...'
$archive = 'dist\stage.zip'
if (Test-Path $archive) { Remove-Item $archive -Force }
& $git archive --format=zip --output $archive HEAD
if ($LASTEXITCODE -ne 0) { exit 1 }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory((Resolve-Path $archive).Path, (Resolve-Path $stage).Path)
Remove-Item $archive -Force
$count = (Get-ChildItem $stage -Recurse -File | Measure-Object).Count
Write-Host ("  files: " + $count)

Write-Host 'adding built binary ...'
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'bin') | Out-Null
if (Test-Path 'build\nav.exe') {
    Copy-Item 'build\nav.exe' (Join-Path $stage 'bin\nav.exe') -Force
} else {
    Write-Host 'warning: build\nav.exe missing (run scripts\build.ps1)' -ForegroundColor Yellow
}
if (Test-Path 'build\navgui.exe') {
    Copy-Item 'build\navgui.exe' (Join-Path $stage 'bin\navgui.exe') -Force
    Write-Host '  + navgui.exe'
} else {
    Write-Host 'note: build\navgui.exe missing (run scripts\build_gui.ps1 to include GUI)' -ForegroundColor Yellow
}

Write-Host 'overlaying final documents ...'
if (Test-Path 'dist\final') {
    $docsDir = Join-Path $stage 'docs'
    if (-not (Test-Path $docsDir)) { New-Item -ItemType Directory -Force -Path $docsDir | Out-Null }
    Get-ChildItem 'dist\final' -File | Where-Object { $_.Extension -in '.docx', '.pdf', '.pptx' } |
        ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $docsDir $_.Name) -Force
            Write-Host ('  ' + $_.Name)
        }
}

$zip = 'dist\' + $name + '-submit.zip'
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Host ('package: ' + $zip) -ForegroundColor Green

$count = (Get-ChildItem $stage -Recurse -File | Measure-Object).Count
Write-Host ("total files in package: " + $count)
Get-ChildItem $stage | Select-Object Name | ForEach-Object { Write-Host ("  - " + $_.Name) }

# Clean the staging directory so dist/ does not accumulate intermediates.
if (Test-Path $stage) {
    try {
        Remove-Item -Recurse -Force $stage
        Write-Host 'staging directory removed.'
    } catch {
        Write-Host 'note: staging directory is in use; remove it manually.' -ForegroundColor Yellow
    }
}
