@echo off
..\..\premake_tb.exe --to=build/vs2022 vs2022
if errorlevel 1 goto error
goto ok

:error
pause

:ok