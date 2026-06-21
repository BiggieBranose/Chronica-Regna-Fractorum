@echo off
setlocal

set "PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;C:\VulkanSDK\1.4.350.0\Bin;%PATH%"
set "VULKAN_SDK=C:\VulkanSDK\1.4.350.0"

rmdir /s /q build 2>nul

cmake -S ./Code -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

cd /d "%~dp0build" || exit /b
call crf_game.exe

endlocal