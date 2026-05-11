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
where OpenCppCoverage.exe >nul 2>&1
if errorlevel 1 (
  echo OpenCppCoverage.exe not found; running ZmeyaTest without coverage.
  .\build\Release\ZmeyaTest.exe
  exit /b %ERRORLEVEL%
)
OpenCppCoverage.exe --sources Zmeya*.* --excluded_sources *googletest* --modules *.exe -- .\build\Release\ZmeyaTest.exe
