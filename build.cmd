@echo off
cd /d "%~dp0"
clang++ -std=c++17 -Os -s -static -municode -mwindows -Wall -ffunction-sections -fdata-sections -Wl,--gc-sections presence.cpp -o TruWorldsCreatorRPC.exe
