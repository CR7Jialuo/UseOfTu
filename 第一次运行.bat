@echo off
chcp 65001 >nul
rem First run helper:
rem   1) drop "Mark of the Web" from every file in this folder
rem   2) start the GUI through run_gui.cmd (which uses a clean local copy)
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "Get-ChildItem -LiteralPath '%~dp0' -Recurse -File -ErrorAction SilentlyContinue | Unblock-File -ErrorAction SilentlyContinue" >nul 2>&1
call "%~dp0run_gui.cmd"
exit /b 0
