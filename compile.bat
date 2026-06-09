@echo off
setlocal

cd /d "%~dp0Code" || exit /b

call compileShaders.bat
if errorlevel 1 exit /b 1

cd /d "%~dp0"

rmdir /s /q build

cmake -S ./Code -B build -G Ninja
cmake --build build

xcopy /e /i /y "%~dp0textures" "%~dp0build\textures" >nul 2>nul
cd /d "%~dp0build" || exit /b
call Chronica_Regna_Fractorum\Chronica_Regna_Fractorum.exe

endlocal