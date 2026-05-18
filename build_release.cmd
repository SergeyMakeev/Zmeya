@echo off
set builddir=build
if not exist %builddir% goto GENERATE
del %builddir% /S /Q
:GENERATE
mkdir %builddir%
cd %builddir%
cmake ..
cd ..
cmake --build .\build\ --config Release
.\build\Release\ZmeyaTest.exe
exit /b %ERRORLEVEL%
