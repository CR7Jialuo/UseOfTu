# Build the thesis report + slides + PDFs.
# Public repo versions are masked (placeholder personal info); real versions go to dist\final.
# Pipeline: figures -> report pass 1 -> locate headings -> report pass 2 (masked + real) -> slides.
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
$pagesJson = 'docs\thesis_pages.json'
New-Item -ItemType Directory -Force -Path $finalDir | Out-Null
New-Item -ItemType Directory -Force -Path 'build' | Out-Null

function Convert-ToPdf([string]$inputFile, [string]$outputFile) {
    if (Test-Path $outputFile) { Remove-Item $outputFile -Force }
    & $node $cli convert --input $inputFile --output $outputFile | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "convert failed: $inputFile" }
}

Write-Host '== 1/7 figures ==' -ForegroundColor Cyan
& $py 'docs\tools\make_figures.py' .
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 2/7 report pass 1 (locate headings) ==' -ForegroundColor Cyan
& $py 'docs\tools\make_thesis.py' --out 'build\pass1.docx'
if ($LASTEXITCODE -ne 0) { exit 1 }
Convert-ToPdf 'build\pass1.docx' 'build\pass1.pdf'
& $py 'docs\tools\find_pages.py' 'build\pass1.pdf' 'docs\toc_entries.json' $pagesJson
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 3/7 report pass 2 (masked -> docs) ==' -ForegroundColor Cyan
& $py 'docs\tools\make_thesis.py' --mask --pages $pagesJson --out $docx
if ($LASTEXITCODE -ne 0) { exit 1 }
Convert-ToPdf $docx $pdf

Write-Host '== 4/7 report pass 2 (real -> dist\final) ==' -ForegroundColor Cyan
$realDocx = Join-Path $finalDir '课程设计报告.docx'
$realPdf = Join-Path $finalDir '课程设计报告.pdf'
& $py 'docs\tools\make_thesis.py' --pages $pagesJson --out $realDocx
if ($LASTEXITCODE -ne 0) { exit 1 }
Convert-ToPdf $realDocx $realPdf

Write-Host '== 5/7 slides (masked -> docs) ==' -ForegroundColor Cyan
& $py 'docs\tools\make_ppt.py' . --mask
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 6/7 slides (real -> dist\final) ==' -ForegroundColor Cyan
$realPptx = Join-Path $finalDir '答辩演示.pptx'
& $py 'docs\tools\make_ppt.py' . --out $realPptx
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host '== 7/7 summaries ==' -ForegroundColor Cyan
Get-ChildItem 'docs' -File | Where-Object { $_.Extension -in '.docx', '.pdf', '.pptx' } |
    Select-Object Name, Length | Format-Table -AutoSize
Get-ChildItem $finalDir -File | Where-Object { $_.Extension -in '.docx', '.pdf', '.pptx' } |
    Select-Object Name, Length | Format-Table -AutoSize

Write-Host 'docs built.' -ForegroundColor Green
