param([switch]$Disable,[switch]$Portable)
$ErrorActionPreference = 'Stop'
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Startup')) 'AudioControl.lnk'
if ($Disable) {
    if (Test-Path -LiteralPath $shortcutPath) { Remove-Item -LiteralPath $shortcutPath }
    Write-Output 'AudioControl startup disabled.'
    return
}
$executable = Join-Path $PSScriptRoot 'build\AudioControl.exe'
if (!(Test-Path -LiteralPath $executable)) { throw 'Build AudioControl first using ./build.ps1.' }
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $executable
$shortcut.Arguments = if ($Portable) { '--portable' } else { '' }
$shortcut.WorkingDirectory = Split-Path $executable
$shortcut.Description = 'AudioControl audio and media shortcuts'
$shortcut.Save()
$saved = $shell.CreateShortcut($shortcutPath)
if ($saved.TargetPath -ne $executable) { throw 'Startup shortcut verification failed.' }
Write-Output "AudioControl will launch at sign-in: $($saved.TargetPath)"
