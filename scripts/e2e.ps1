# End-to-end checks for the CLI (ASCII only, Windows PowerShell).
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$exe = '.\build\nav.exe'
if (-not (Test-Path $exe)) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$script:fail = 0
function Check([string]$name, [scriptblock]$body) {
    try {
        $ok = & $body
        if ($ok) {
            Write-Host "  OK   $name" -ForegroundColor Green
        } else {
            Write-Host "  FAIL $name" -ForegroundColor Red
            $script:fail++
        }
    } catch {
        Write-Host "  FAIL $name ($_)" -ForegroundColor Red
        $script:fail++
    }
}

Write-Host "e2e: $exe"

Check 'list 显示地点' { (& $exe list | Out-String) -match '图书馆' }

Check 'route JSON 路径正确' {
    $j = (& $exe route 16 4 --json | Out-String).Trim()
    ($j -match '"count":4') -and ($j -match '"nodes":\[16,14,22,4\]')
}

Check 'route 按时间口径' {
    $j = (& $exe route '东大门' '图书馆' --by time --json | Out-String).Trim()
    ($j -match '"criterion":"time"') -and ($j -match '"distance_m"')
}

Check 'route 起点=终点' {
    $j = (& $exe route 1 1 --json | Out-String).Trim()
    $j -match '"count":1'
}

Check 'route 非法地点返回 2' {
    & $exe route 1 999 --json *> $null
    ($LASTEXITCODE -eq 2)
}

Check 'near 最近食堂' { (& $exe near '宿舍A区' canteen | Out-String) -match '学生一食堂' }

Check 'tour 多点巡游' { (& $exe tour '东大门' '图书馆' '第一教学楼' | Out-String) -match '游览顺序' }

Check 'critical 割点与桥' {
    $o = (& $exe critical | Out-String)
    ($o -match '割点') -and ($o -match '桥') -and ($o -match '观海亭')
}

Check 'mst 双算法一致' { (& $exe mst | Out-String) -match 'Prim' }

Check 'allpairs 矩阵' { (& $exe allpairs --limit 4 | Out-String) -match 'Floyd-Warshall' }

Check 'map ASCII 地图' {
    $o = (& $exe map | Out-String)
    ($o -match '东大门') -and ($o -match '图例')
}

Check 'check 通过（路网连通）' {
    & $exe check *> $null
    ($LASTEXITCODE -eq 0)
}

Check 'bench 可运行' { (& $exe bench --nodes 40 --repeat 2 | Out-String) -match 'Dijkstra' }

Check 'export 与仓库数据一致' {
    if (Test-Path 'build\e2e_data') { Remove-Item -Recurse -Force 'build\e2e_data' }
    & $exe export build\e2e_data *> $null
    $a = Get-Content 'data\campus\spots.csv' -Encoding UTF8 | Where-Object { $_ -notmatch '^#' }
    $b = Get-Content 'build\e2e_data\spots.csv' -Encoding UTF8 | Where-Object { $_ -notmatch '^#' }
    $c = Get-Content 'data\campus\roads.csv' -Encoding UTF8 | Where-Object { $_ -notmatch '^#' }
    $d = Get-Content 'build\e2e_data\roads.csv' -Encoding UTF8 | Where-Object { $_ -notmatch '^#' }
    (($a -join "`n") -eq ($b -join "`n")) -and (($c -join "`n") -eq ($d -join "`n"))
}

Write-Host ""
if ($script:fail -eq 0) {
    Write-Host 'e2e passed.' -ForegroundColor Green
    exit 0
}
Write-Host "e2e failed: $($script:fail) case(s)." -ForegroundColor Red
exit 1
