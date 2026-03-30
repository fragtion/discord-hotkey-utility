@echo off
set SRC=discord-hotkey.c
set FLAGS=-lhid -lsetupapi -lwinhttp -lws2_32 -lgdi32

REM Set paths for native MinGW-w64
set MINGW64=C:\ProgramData\mingw64\mingw64\bin

echo Building x64...
%MINGW64%\x86_64-w64-mingw32-gcc -o discord-hotkey-x64.exe %SRC% %FLAGS%
if %errorlevel%==0 (echo x64 OK) else (echo x64 FAILED)

echo Done.
pause