@echo off
setlocal
set "PATH=C:\msys64\mingw64\bin;%PATH%"
cmake -S "%~dp0." -B "%~dp0build-static-next" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64/qt6-static
if errorlevel 1 exit /b %errorlevel%
cmake --build "%~dp0build-static-next" -j 4
exit /b %errorlevel%
