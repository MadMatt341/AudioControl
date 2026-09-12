param([Parameter(Mandatory)][ValidatePattern('^\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?$')][string]$Version)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/test.ps1"
& "$PSScriptRoot/build.ps1" -OutputDirectory 'build/release'
$output = Join-Path $PSScriptRoot 'build/releases'
New-Item -ItemType Directory -Force $output | Out-Null
$name = "AudioControl-$Version-win-x64"
$zip = Join-Path $output "$name.zip"
if (Test-Path -LiteralPath $zip) { throw "Release already exists: $zip" }
$stage = Join-Path $output ([Guid]::NewGuid().ToString())
$folder = Join-Path $stage $name
New-Item -ItemType Directory -Force (Join-Path $folder 'build') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'build/release/AudioControl.exe') -Destination (Join-Path $folder 'build')
foreach ($file in @('README.md', 'LICENSE', 'startup.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $folder
}
Compress-Archive -LiteralPath $folder -DestinationPath $zip
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zip.sha256" -Value "$hash  $name.zip" -Encoding ascii
Write-Output "Packaged $zip"
