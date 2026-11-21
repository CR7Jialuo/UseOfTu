@echo off
chcp 65001 >nul
rem First-run helper: remove Mark-of-the-Web (SmartScreen block) then start the GUI.
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -LiteralPath '%~dp0' -Recurse -File -ErrorAction SilentlyContinue | Unblock-File -ErrorAction SilentlyContinue"
if exist "%~dp0build\navgui.exe" (
    start "" "%~dp0build\navgui.exe"
) else (
    echo 未找到 build\navgui.exe，请先运行 build_gui.cmd 编译。
    pause
)
exit /b 0
