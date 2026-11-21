@echo off
chcp 65001 >nul
setlocal
rem Launch the GUI from a clean local copy.
rem Rewriting the binary drops the "Mark of the Web" stream, so SmartScreen
rem stops treating it as a downloaded app (that is what shows the blue
rem "Windows protected your PC" dialog).
set "ROOT=%~dp0"
set "SRC=%ROOT%build\navgui.exe"
set "DSTDIR=%LOCALAPPDATA%\CampusNav"
set "DST=%DSTDIR%\navgui.exe"

if not exist "%SRC%" (
    if exist "%ROOT%build_gui.cmd" (
        echo [i] build\navgui.exe not found, building first ...
        call "%ROOT%build_gui.cmd"
    )
)
if not exist "%SRC%" (
    echo [x] build\navgui.exe still missing. Run build_gui.cmd manually.
    pause
    exit /b 1
)
if not exist "%DSTDIR%" mkdir "%DSTDIR%" >nul 2>&1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$s='%SRC%'; $d='%DST%'; if ((Test-Path $d) -and ((Get-Item $d).LastWriteTime -ge (Get-Item $s).LastWriteTime)) { exit 0 }; [IO.File]::WriteAllBytes($d, [IO.File]::ReadAllBytes($s))" >nul 2>&1

if not exist "%DST%" (
    echo [!] clean copy failed, launching the original exe.
    cd /d "%ROOT%"
    start "" "%SRC%" --data "%ROOT%data\campus_real"
    exit /b 0
)

cd /d "%ROOT%"
start "" "%DST%" --data "%ROOT%data\campus_real"
endlocal
exit /b 0
