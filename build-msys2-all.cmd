@echo off
set SRC=discord-hotkey.c
set FLAGS=-lhid -lsetupapi -lwinhttp -lws2_32 -lgdi32
set OPTIMIZE=-s -Os -ffunction-sections -fdata-sections -Wl,--gc-sections

set MSYS2=C:\tools\msys64\usr\bin\bash.exe

REM Convert current dir to MSYS format
set CURDIR=%CD%
set CURDIR=%CURDIR:\=/%
set CURDIR=/%CURDIR:~0,1%%CURDIR:~2%

echo Building x64 (optimized)...
%MSYS2% -lc "cd %CURDIR% && export PATH=/mingw64/bin:\$PATH && x86_64-w64-mingw32-gcc %OPTIMIZE% -o discord-hotkey-x64.exe %SRC% %FLAGS%"
if %errorlevel%==0 (echo x64 OK) else (echo x64 FAILED)

echo Building x86 (optimized)...
%MSYS2% -lc "cd %CURDIR% && export PATH=/mingw32/bin:\$PATH && i686-w64-mingw32-gcc %OPTIMIZE% -o discord-hotkey-x86.exe %SRC% %FLAGS%"
if %errorlevel%==0 (echo x86 OK) else (echo x86 FAILED)

echo Building arm64 (optimized)...
%MSYS2% -lc "cd %CURDIR% && export PATH=/clang64/bin:\$PATH && clang -target aarch64-w64-windows-gnu --sysroot=/clangarm64 -fuse-ld=lld -resource-dir=/clangarm64/lib/clang/22 %OPTIMIZE% -I/clangarm64/include -L/clangarm64/lib -L/clangarm64/aarch64-w64-mingw32/lib -o discord-hotkey-arm64.exe %SRC% %FLAGS%"
if %errorlevel%==0 (echo arm64 OK) else (echo arm64 FAILED)

echo Done.
pause