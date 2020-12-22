@echo off
..\..\premake5.exe --systemscript=../../tiki_build.lua --to=build/android --os=android vs2019
if errorlevel 1 goto error
goto ok

:error
pause

:ok