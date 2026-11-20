# Build report + slides + PDFs (ASCII only, except file names).
# Public repo versions are masked (placeholder personal info); real versions go to dist\final.
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$py = $env:PYTHON
if (-not $py) {
    $py = 'C:\Users\ZhuanZ\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe'
}
if (-not (Test-Path $py)) {
    Write-Host 'ERROR: python not found. Set PYTHON env var.' -ForegroundColor Red
    exit 1
}
$node = 'D:\program\deepseek\resources\runtime\primary-runtime\dependencies\node\bin\node.exe'
$cli = 'D:\program\deepseek\resources\app.asar.unpacked\dsh\node_modules\@deepseek-ai\libreoffice-kit\lib\cli.js'
$env:PYTHONIOENCODING = 'utf-8'
$env:PYTHONPATH = Join-Path (Split-Path -Parent $root) '.tools\pylib'

$docx = 'docs\课程设计报告.docx'
$pdf = 'docs\课程设计报告.pdf'
$finalDir = 'dist\final'
New-Item -ItemType Directory -Force -Path $finalDir | Out-Null

function Convert-ToPdf([string]$inputFile, [string]$outputFile) {
    if (Test-Path $outputFile) { Remove-Item $outputFile -Force }
    & $node $cli convert --input $inputFile --output $outputFile | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "convert failed: $inputFile" }
}

Write-Host '== 1/6 figures ==' -ForegroundColor Cyan
& $py 'docs\tools\make_figures.py' .
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 2/6 report pass 1 (masked) ==' -ForegroundColor Cyan
& $py 'docs\tools\make_report.py' . --mask
if ($LASTEXITCODE -ne 0) { exit 1 }
Convert-ToPdf $docx $pdf

Write-Host '== 3/6 locate headings ==' -ForegroundColor Cyan
& $py 'docs\tools\find_pages.py' $pdf 'docs\toc_entries.json' 'docs\toc_pages.json' 2>$null
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 4/6 report pass 2 (masked -> public, real -> dist\final) ==' -ForegroundColor Cyan
& $py 'docs\tools\make_report.py' . --mask --pages 'docs\toc_pages.json'
if ($LASTEXITCODE -ne 0) { exit 1 }
Convert-ToPdf $docx $pdf

$realDocx = Join-Path $finalDir '课程设计报告.docx'
$realPdf = Join-Path $finalDir '课程设计报告.pdf'
& $py 'docs\tools\make_report.py' . --pages 'docs\toc_pages.json' --out $realDocx
if ($LASTEXITCODE -ne 0) { exit 1 }
Convert-ToPdf $realDocx $realPdf

Write-Host '== 5/6 slides (masked + real) ==' -ForegroundColor Cyan
& $py 'docs\tools\make_ppt.py' . --mask
if ($LASTEXITCODE -ne 0) { exit 1 }
$realPptx = Join-Path $finalDir '答辩演示.pptx'
& $py 'docs\tools\make_ppt.py' . --out $realPptx
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 6/6 summaries ==' -ForegroundColor Cyan
Get-ChildItem 'docs' -File | Where-Object { $_.Extension -in '.docx', '.pdf', '.pptx' } |
    Select-Object Name, Length | Format-Table -AutoSize
Get-ChildItem $finalDir -File | Where-Object { $_.Extension -in '.docx', '.pdf', '.pptx' } |
    Select-Object Name, Length | Format-Table -AutoSize

Write-Host 'docs built.' -ForegroundColor Green
