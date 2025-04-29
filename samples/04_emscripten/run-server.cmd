@ECHO OFF

for %%F in ("%EMSDK_NODE%") do set dirname=%%~dpF

%dirname%\http-server.cmd . -p3000 -c-1