@echo off
set builddir=build
if not exist %builddir% goto GENERATE
del %builddir% /S /Q
:GENERATE
mkdir %builddir%
cd %builddir%
cmake ../
cmake --build . --config Release
cd ..



