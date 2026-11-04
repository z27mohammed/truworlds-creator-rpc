@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0autostart.ps1"
start "" "%~dp0TruWorldsCreatorRPC.exe"
echo Running. Open TruWorlds Creator and check your Discord profile.
pause
