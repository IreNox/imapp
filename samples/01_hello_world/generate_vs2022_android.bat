@echo off
..\..\premake_tb.exe --to=build/vs2022_android --os=android vs2022
if errorlevel 1 goto error
goto ok

:error
pause

:ok