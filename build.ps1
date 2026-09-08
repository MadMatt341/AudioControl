param([switch]$Tests, [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
if (!$OutputDirectory) { $OutputDirectory = if ($Tests) { 'build\tests' } else { 'build' } }
if (![IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory = Join-Path $PSScriptRoot $OutputDirectory }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ desktop build tools first.' }
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Install Visual Studio C++ desktop build tools first.' }
$devcmd = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
# Source/output paths are passed as individual arguments to cl.exe.
$compilerEnvironment = & cmd.exe /d /s /c "call `"$devcmd`" -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Unable to initialize the C++ toolchain.' }
foreach ($line in $compilerEnvironment) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
}
# Some hosts inherit both PATH and Path; preserve the developer bootstrap PATH.
$developerPath = $compilerEnvironment | Where-Object { $_ -cmatch '^PATH=' } | Select-Object -First 1
if ($developerPath) { $env:Path = $developerPath.Substring(5) }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$sources = @('AudioControl.cpp','AudioDevices.cpp','Overlay.cpp','MediaControl.cpp','Shortcuts.cpp','Settings.cpp')
if ($Tests) { $sources += 'AudioControlTests.cpp' }
$sources = $sources | ForEach-Object { Join-Path $PSScriptRoot $_ }
$binaryName = if ($Tests) { 'AudioControlTests.exe' } else { 'AudioControl.exe' }
$subsystem = if ($Tests) { '/SUBSYSTEM:CONSOLE' } else { '/SUBSYSTEM:WINDOWS' }
$compilerArgs = @('/nologo','/std:c++20','/O1','/MT','/EHsc','/W4','/WX','/utf-8','/guard:cf','/D_WIN32_WINNT=0x0A00',"/Fo:$OutputDirectory/", "/Fe:$OutputDirectory/$binaryName")
& cl.exe @compilerArgs @sources /link $subsystem /guard:cf /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /OPT:REF /OPT:ICF /MANIFEST:EMBED "/MANIFESTINPUT:$PSScriptRoot/AudioControl.manifest" user32.lib gdi32.lib shell32.lib ole32.lib uuid.lib windowsapp.lib
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
$headers = & dumpbin.exe /headers (Join-Path $OutputDirectory $binaryName)
if ($LASTEXITCODE -ne 0 -or !($headers -match 'Control Flow Guard')) { throw 'Built binary is missing Control Flow Guard.' }
Write-Output "Built $OutputDirectory\$binaryName (CFG, ASLR and NX enabled)."
