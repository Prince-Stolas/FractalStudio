@echo off
setlocal
set "PATH=C:\msys64\mingw64\bin;%PATH%"
"%~dp0build-dynamic\fractal.exe" %*
exit /b %errorlevel%
