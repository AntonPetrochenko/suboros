@echo off
call "%~dp0build.bat"
if errorlevel 1 exit /b 1
start "" "E:\Portable Software\mesen\Mesen.exe" "%~dp0build\suboros.nes"
