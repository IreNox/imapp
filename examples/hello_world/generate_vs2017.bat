@echo off
..\..\premake5.exe --systemscript=../../tiki_build.lua --to=build/vs2017 vs2017
if errorlevel 1 goto error
goto ok

:error
pause

:ok