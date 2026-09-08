param([switch]$WindowsIntegration)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\build.ps1" -Tests
if ($WindowsIntegration) { & "$PSScriptRoot\build\tests\AudioControlTests.exe" --windows-integration }
else { & "$PSScriptRoot\build\tests\AudioControlTests.exe" }
if ($LASTEXITCODE -ne 0) { throw "Tests failed: $LASTEXITCODE" }
