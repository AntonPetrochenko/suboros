@echo off
setlocal

set CC65=%~dp0tools\cc65\bin\cc65.exe
set CA65=%~dp0tools\cc65\bin\ca65.exe
set LD65=%~dp0tools\cc65\bin\ld65.exe
set SRC=%~dp0src
set BUILD=%~dp0build
set CFG=%~dp0suboros.cfg
set OUT=%BUILD%\suboros.nes

if not exist "%BUILD%" mkdir "%BUILD%"

echo Building user programs...
call "%~dp0programs\build.bat"
if errorlevel 1 goto fail

echo Building ROM filesystem...
python "%~dp0tools\mkfs.py" "%~dp0in_fs" "%BUILD%"
if errorlevel 1 goto fail

echo Assembling...

%CA65% -I "%SRC%" -o "%BUILD%\header.o"   "%SRC%\header.asm"
if errorlevel 1 goto fail

%CA65% -I "%SRC%" -o "%BUILD%\zp.o"       "%SRC%\zp.asm"
if errorlevel 1 goto fail

%CA65% -I "%SRC%" -o "%BUILD%\startup.o"  "%SRC%\startup.asm"
if errorlevel 1 goto fail

%CA65% -I "%SRC%" -o "%BUILD%\nmi.o"      "%SRC%\nmi.asm"
if errorlevel 1 goto fail

%CA65% -o "%BUILD%\fs_rom_data.o" "%BUILD%\fs_rom_data.asm"
if errorlevel 1 goto fail

echo Compiling C...

%CC65% -O -I "%SRC%" -o "%BUILD%\main.s" "%SRC%\main.c"
if errorlevel 1 goto fail

%CA65% -o "%BUILD%\main.o" "%BUILD%\main.s"
if errorlevel 1 goto fail

%CC65% -O -I "%SRC%" -o "%BUILD%\syscalls.s" "%SRC%\syscalls.c"
if errorlevel 1 goto fail

%CA65% -o "%BUILD%\syscalls.o" "%BUILD%\syscalls.s"
if errorlevel 1 goto fail

%CC65% -O -I "%SRC%" -o "%BUILD%\fs.s" "%SRC%\fs.c"
if errorlevel 1 goto fail

%CA65% -o "%BUILD%\fs.o" "%BUILD%\fs.s"
if errorlevel 1 goto fail

%CC65% -O -I "%SRC%" -o "%BUILD%\proc.s" "%SRC%\proc.c"
if errorlevel 1 goto fail

%CA65% -o "%BUILD%\proc.o" "%BUILD%\proc.s"
if errorlevel 1 goto fail

%CC65% -O -I "%SRC%" -o "%BUILD%\alloc.s" "%SRC%\alloc.c"
if errorlevel 1 goto fail

%CA65% -o "%BUILD%\alloc.o" "%BUILD%\alloc.s"
if errorlevel 1 goto fail

%CC65% -O -I "%SRC%" -o "%BUILD%\ppu.s" "%SRC%\ppu.c"
if errorlevel 1 goto fail

%CA65% -o "%BUILD%\ppu.o" "%BUILD%\ppu.s"
if errorlevel 1 goto fail

echo Linking...

%LD65% -C "%CFG%" -o "%OUT%" ^
    "%BUILD%\header.o"      ^
    "%BUILD%\zp.o"          ^
    "%BUILD%\startup.o"     ^
    "%BUILD%\nmi.o"         ^
    "%BUILD%\fs_rom_data.o" ^
    "%BUILD%\main.o"        ^
    "%BUILD%\syscalls.o"    ^
    "%BUILD%\fs.o"          ^
    "%BUILD%\proc.o"        ^
    "%BUILD%\alloc.o"       ^
    "%BUILD%\ppu.o"         ^
    "%~dp0tools\cc65\lib\none.lib"
if errorlevel 1 goto fail

echo.
echo OK: %OUT%
goto :eof

:fail
echo.
echo FAILED.
exit /b 1
