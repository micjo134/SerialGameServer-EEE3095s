# ========================================================
#  PowerShell Test Runner for Ceedling
# ========================================================
param (
    [string]$Target = "test:all"
)

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host " Running Ceedling Host-Based Unit Tests: $Target" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

$ceedlingCmd = Get-Command ceedling -ErrorAction SilentlyContinue

if (-not $ceedlingCmd) {
    Write-Host "[ERROR] 'ceedling' was not found in your system PATH." -ForegroundColor Red
    Write-Host ""
    Write-Host "If Ruby is already installed, run:" -ForegroundColor Yellow
    Write-Host "   gem install ceedling" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "If Ruby is not installed, install Ruby+Devkit from:" -ForegroundColor Yellow
    Write-Host "   https://rubyinstaller.org/" -ForegroundColor Yellow
    exit 1
}

ceedling $Target
