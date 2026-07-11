@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALAR_GLM.ps1"
if errorlevel 1 (
    echo.
    echo ERROR: No se pudo instalar GLM.
    pause
    exit /b 1
)
echo.
echo GLM instalado correctamente.
pause
