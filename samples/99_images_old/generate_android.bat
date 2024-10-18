@echo off
..\..\premake_tb.exe --to=build/android --os=android vs2019
if errorlevel 1 goto error
goto ok

:error
pause

:ok