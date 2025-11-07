@echo off
REM KanchoNet 빌드 스크립트 (Windows)

echo =======================================
echo   KanchoNet Build Script (Windows)
echo =======================================
echo.

REM 빌드 설정 (기본값: Release)
set BUILD_CONFIG=%1
if "%BUILD_CONFIG%"=="" set BUILD_CONFIG=Release

echo Build configuration: %BUILD_CONFIG%
echo Platform: x64
echo.

REM Visual Studio 2022로 빌드
echo Building with MSBuild...
msbuild KanchoNet.sln /p:Configuration=%BUILD_CONFIG% /p:Platform=x64 /m

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo =======================================
echo   Build completed!
echo =======================================
echo.
echo Binaries location:
echo   - Library: bin\%BUILD_CONFIG%\KanchoNet.lib
echo   - Examples: bin\%BUILD_CONFIG%\
echo.
echo To run example servers:
echo   bin\%BUILD_CONFIG%\EchoServer.exe
echo   bin\%BUILD_CONFIG%\ChatServer.exe
echo   bin\%BUILD_CONFIG%\ProtobufServer.exe
echo.

