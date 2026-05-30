@echo off
echo Compiling Pitch Shifter Project...

:: Run the g++ command
g++ -o task2.exe task2.2.cpp smbPitchShift.cpp -lportaudio
:: Check if it built successfully
if %errorlevel% equ 0 (
    echo.
    echo ====================================
    echo Build Successful! Run task2.exe
    echo ====================================
) else (
    echo.
    echo ====================================
    echo Build Failed! Check errors above.
    echo ====================================
)
pause