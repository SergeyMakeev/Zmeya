@echo off
set builddir=build
if not exist %builddir% goto GENERATE
del %builddir% /S /Q
:GENERATE
mkdir %builddir%
cd %builddir%
cmake ..
cd ..
cmake --build .\build\ --config Debug
OpenCppCoverage.exe --sources Zmeya*.* --excluded_sources *googletest* --modules *.exe -- .\build\Debug\ZmeyaTest.exe
