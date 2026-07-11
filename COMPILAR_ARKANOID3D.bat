@echo off
cd /d "%~dp0"
echo Compilando Arkanoid 3D en modo retenido...
g++ -std=c++17 Arkanoid3D\main.cpp glad.c -Iinclude -Llib -lglfw3dll -lopengl32 -lgdi32 -o Arkanoid3D\Arkanoid3D.exe
if errorlevel 1 (
    echo.
    echo ERROR: No se pudo compilar Arkanoid 3D.
    echo Verifica que g++ este instalado y que existan include, lib y glad.c.
    pause
    exit /b 1
)
copy lib\glfw3.dll Arkanoid3D\glfw3.dll >nul
echo.
echo Compilacion terminada correctamente.
pause
