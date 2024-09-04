@echo off
..\..\premake_tb.exe --to=build/vs2022_linux --os=linux --cc=gcc vs2022
if errorlevel 1 goto error
goto ok

:error
pause

:ok