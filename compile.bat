@echo off
setlocal

set "PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;C:\VulkanSDK\1.4.350.0\Bin;%PATH%"
set "VULKAN_SDK=C:\VulkanSDK\1.4.350.0"

cd /d "%~dp0Code" || exit /b

call compileShaders.bat
if errorlevel 1 exit /b 1

cd /d "%~dp0"

rmdir /s /q build

cmake -S ./Code -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

xcopy /s /e /y "%~dp0Code\textures" "%~dp0build\Chronica_Regna_Fractorum\textures\" >nul

cd /d "%~dp0build\Chronica_Regna_Fractorum" || exit /b
call Chronica_Regna_Fractorum.exe

endlocal