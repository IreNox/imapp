@echo off
..\..\premake_tb.exe --to=build/vs2017 vs2017
if errorlevel 1 goto error
goto ok

:error
pause

:ok