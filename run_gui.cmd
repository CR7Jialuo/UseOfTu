@echo off
chcp 65001 >nul
if not exist "build\navgui.exe" call "%~dp0build_gui.cmd"
start "" "build\navgui.exe" --data data\campus_real
exit /b 0
