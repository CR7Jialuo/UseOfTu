@echo off
chcp 65001 >nul
setlocal
rem Launch the GUI from a clean local copy.
rem  * works both in the source tree (build\navgui.exe) and in the submission
rem    package (bin\navgui.exe);
rem  * rewriting the binary drops the "Mark of the Web" stream, so SmartScreen
rem    no longer shows "Windows protected your PC".
set "ROOT=%~dp0"
set "SRC="
if exist "%ROOT%bin\navgui.exe"   set "SRC=%ROOT%bin\navgui.exe"
if exist "%ROOT%build\navgui.exe" set "SRC=%ROOT%build\navgui.exe"

if not defined SRC (
    if exist "%ROOT%build_gui.cmd" (
        echo [i] navgui.exe not found, trying to build ...
        call "%ROOT%build_gui.cmd"
        if exist "%ROOT%build\navgui.exe" set "SRC=%ROOT%build\navgui.exe"
    )
)

if not defined SRC (
    echo [x] navgui.exe not found.
    echo     Expected at: bin\navgui.exe  or  build\navgui.exe
    echo     Source tree? run build_gui.cmd first - needs MinGW gcc.
    pause
    exit /b 1
)

set "DATA=%ROOT%data\campus_real"
if not exist "%DATA%\spots.csv" set "DATA=%ROOT%data\campus"

set "DSTDIR=%LOCALAPPDATA%\CampusNav"
set "DST=%DSTDIR%\navgui.exe"
if not exist "%DSTDIR%" mkdir "%DSTDIR%" >nul 2>&1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$s='%SRC%'; $d='%DST%'; if ((Test-Path $d) -and ((Get-Item $d).LastWriteTime -ge (Get-Item $s).LastWriteTime)) { exit 0 }; [IO.File]::WriteAllBytes($d, [IO.File]::ReadAllBytes($s))" >nul 2>&1

if defined NAVGUI_DRYRUN (
    echo SRC=%SRC%
    echo DATA=%DATA%
    echo DST=%DST%
    exit /b 0
)

cd /d "%ROOT%"
if exist "%DST%" (
    start "" "%DST%" --data "%DATA%"
) else (
    start "" "%SRC%" --data "%DATA%"
)
endlocal
exit /b 0
