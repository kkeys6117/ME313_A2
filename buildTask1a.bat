@echo off
echo Compiling Pitch Shifter Project...

:: Run the g++ command
g++ -o task1a.exe task2.1a.cpp -lportaudio
:: Check if it built successfully
if %errorlevel% equ 0 (
    echo.
    echo ====================================
    echo Build Successful! Run task1a.exe
    echo ====================================
) else (
    echo.
    echo ====================================
    echo Build Failed! Check errors above.
    echo ====================================
)
pause