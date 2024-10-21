@ECHO OFF

for %%F in ("%EMSDK_NODE%") do set dirname=%%~dpF

%dirname%\http-server.cmd build