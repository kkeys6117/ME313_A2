@echo off
echo Compiling Pitch Shifter Project...

:: Run the g++ command
g++ -O3 -std=c++17 task2.3.cpp PitchShifterApp.cpp smbPitchShift.cpp -Wno-stringop-overflow -lportaudio -o pitch_shifter.exe
:: Check if it built successfully
if %errorlevel% equ 0 (
    echo.
    echo ====================================
    echo Build Successful! Run task3.exe
    echo ====================================
) else (
    echo.
    echo ====================================
    echo Build Failed! Check errors above.
    echo ====================================
)
pause