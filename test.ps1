# Set error action to stop
$ErrorActionPreference = "Stop"

$testRunners = Get-ChildItem -Path "build_win\builds\Release\*test_runner.exe"
if ($testRunners.Count -eq 0) {
    # Check alternate location if name without _runner is present
    $testRunners = Get-ChildItem -Path "build_win\builds\Release\test_runner.exe"
}

Write-Host "Running Windows Test Suite..." -ForegroundColor Green

$failedCount = 0
$passedCount = 0

foreach ($runner in $testRunners) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host " Executing: $($runner.Name)" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    & $runner.FullName
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED: $($runner.Name)" -ForegroundColor Red
        $failedCount++
    } else {
        $passedCount++
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Yellow
Write-Host " Summary: $passedCount Suite(s) Passed, $failedCount Suite(s) Failed" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow

if ($failedCount -gt 0) {
    exit 1
}
