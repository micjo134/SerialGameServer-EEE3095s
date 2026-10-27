@echo off
setlocal
echo ========================================================
echo  Running Ceedling Host-Based Unit Tests
echo ========================================================

where ceedling >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 'ceedling' is not found in your system PATH.
    echo.
    echo If you already installed Ruby, run:
    echo    gem install ceedling
    echo.
    echo If Ruby is not installed, download Ruby+Devkit from:
    echo    https://rubyinstaller.org/
    exit /b 1
)

if "%~1"=="" (
    echo Running all tests: ceedling test:all
    ceedling test:all
) else (
    echo Running target: ceedling %*
    ceedling %*
)
