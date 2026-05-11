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
if errorlevel 1 exit /b %ERRORLEVEL%
where OpenCppCoverage.exe >nul 2>&1
if errorlevel 1 (
  echo OpenCppCoverage.exe not found; full suite already ran above.
  exit /b 0
)
echo Running OpenCppCoverage (skipping tests that are impractically slow under instrumentation: large incremental hash, ListTest 1M nodes in Release, etc.).
OpenCppCoverage.exe --sources Zmeya*.* --excluded_sources *googletest* --modules *.exe -- .\build\Release\ZmeyaTest.exe --gtest_filter=-ZmeyaTestSuite.Coverage_P1_LargeNIncrementalHashMapVsGolden:ZmeyaTestSuite.Coverage_P13_IncrementalHashWallTimeBoundReleaseOnly:ZmeyaTestSuite.ListTest
exit /b %ERRORLEVEL%
