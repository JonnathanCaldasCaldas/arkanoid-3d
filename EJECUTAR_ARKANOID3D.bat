@echo off
cd /d "%~dp0"
if not exist Arkanoid3D\Arkanoid3D.exe (
    echo No existe Arkanoid3D\Arkanoid3D.exe
    echo Primero ejecuta COMPILAR_ARKANOID3D.bat
    pause
    exit /b 1
)
Arkanoid3D\Arkanoid3D.exe
