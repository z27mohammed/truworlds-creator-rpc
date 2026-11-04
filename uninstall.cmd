@echo off
call "%~dp0stop.cmd"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0autostart.ps1" -Remove
pause
