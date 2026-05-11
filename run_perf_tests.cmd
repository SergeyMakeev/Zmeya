@echo off
cd /d "%~dp0"
cmake -S . -B build-bench -DZMEYA_BUILD_BENCHMARKS=ON -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b %ERRORLEVEL%
cmake --build build-bench --config Release --target ZmeyaBench
if errorlevel 1 exit /b %ERRORLEVEL%
build-bench\Release\ZmeyaBench.exe %*
exit /b %ERRORLEVEL%
