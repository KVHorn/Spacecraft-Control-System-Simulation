@echo off
REM -- Simple one-click build for Windows (requires CMake + Visual Studio + git) --
setlocal

echo.
echo === Configuring SolarSystemSim with CMake ===
cmake -S . -B build
if errorlevel 1 (
    echo.
    echo CMake configure FAILED. Common causes:
    echo  - git is not on PATH ^(needed for FetchContent to download dependencies^)
    echo  - CMake version too old ^(need 3.16+^)
    echo  - Visual Studio / Build Tools not installed
    echo.
    exit /b 1
)

echo.
echo === Building Release ===
cmake --build build --config Release -j
if errorlevel 1 (
    echo.
    echo Build FAILED.
    exit /b 1
)

echo.
echo === Build successful ===
echo Executable: bin\SolarSystemSim.exe
echo.
echo Remember to put your planet texture files in the textures\ folder.
echo See README.md for the expected filenames.
endlocal
