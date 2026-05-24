@echo off
setlocal

set CA65=%~dp0..\tools\cc65\bin\ca65.exe
set LD65=%~dp0..\tools\cc65\bin\ld65.exe
set PRG_BUILD=%~dp0..\build\prg
set IN_FS=%~dp0..\in_fs\0

if not exist "%PRG_BUILD%" mkdir "%PRG_BUILD%"
if not exist "%IN_FS%"     mkdir "%IN_FS%"

set HAD_ERROR=0

for /d %%D in ("%~dp0*") do (
    if /i not "%%~nxD"=="include" (
        call :build_one "%%D" "%%~nxD"
        if errorlevel 1 set HAD_ERROR=1
    )
)

if "%HAD_ERROR%"=="1" (
    echo.
    echo Programs build FAILED.
    exit /b 1
)
echo Programs build OK.
goto :eof

:build_one
setlocal
set PRG_DIR=%~1
set NAME=%~2
set CFG=%PRG_DIR%\%NAME%.cfg
set SRC=%PRG_DIR%\main.asm
set OBJ=%PRG_BUILD%\%NAME%.o
set OUT=%PRG_BUILD%\%NAME%.prg
set DEST=%IN_FS%\%NAME%.prg

echo Building program: %NAME%

if not exist "%CFG%" ( echo   ERROR: missing %CFG% & exit /b 1 )
if not exist "%SRC%" ( echo   ERROR: missing %SRC% & exit /b 1 )

:: Reject cfg files that place segments in OS RAM ($0200-$07FF).
:: Simplest indicator: presence of a BSS segment.
findstr /i "BSS" "%CFG%" >nul 2>&1
if not errorlevel 1 (
    echo   ERROR: %CFG% contains a BSS segment - user programs must not occupy OS RAM.
    exit /b 1
)

%CA65% -I "%~dp0include" -o "%OBJ%" "%SRC%"
if errorlevel 1 exit /b 1

%LD65% -C "%CFG%" -o "%OUT%" "%OBJ%"
if errorlevel 1 exit /b 1

copy /y /b "%OUT%" "%DEST%" >nul
if errorlevel 1 ( echo   ERROR: copy to %DEST% failed & exit /b 1 )
echo   -^> %DEST%
endlocal
exit /b 0
