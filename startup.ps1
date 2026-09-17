[CmdletBinding(DefaultParameterSetName='Enable')]
param(
    [Parameter(ParameterSetName='Disable', Mandatory=$true)][switch]$Disable,
    [Parameter(ParameterSetName='Status', Mandatory=$true)][switch]$Status,
    [Parameter(ParameterSetName='Enable')][switch]$Portable,
    [Parameter(ParameterSetName='Enable')][string]$ExecutablePath
)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$interactiveUser = $identity.Name
$sid = $identity.User.Value
$sessionId = (Get-Process -Id $PID).SessionId
$explorers = @(Get-CimInstance Win32_Process -Filter "Name='explorer.exe'" | Where-Object SessionId -eq $sessionId)
if (!$explorers.Count) { throw 'Run this script from your signed-in Windows desktop session.' }
foreach ($explorer in $explorers) {
    $owner = Invoke-CimMethod -InputObject $explorer -MethodName GetOwnerSid
    if ($owner.ReturnValue -ne 0 -or $owner.Sid -ne $sid) {
        throw 'Current process and interactive user differ. Run this script as the signed-in desktop user, outside a sandbox or alternate account.'
    }
}
$runKey = "$sid\Software\Microsoft\Windows\CurrentVersion\Run"
$approvalRoot = "$sid\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved"
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Startup')) 'AudioControl.lnk'
function RegistryCall($method, $key, $name, $extra = @{}) {
    $arguments = @{ hDefKey=[uint32]2147483651; sSubKeyName=$key }
    if ($null -ne $name) { $arguments.sValueName=$name }
    foreach ($entry in $extra.GetEnumerator()) { $arguments[$entry.Key]=$entry.Value }
    # StdRegProv can return 1 for an absent value. Enumerate first to distinguish
    # absence from read failures and unsupported registry types.
    if ($method -in @('GetStringValue','GetBinaryValue')) {
        $enumerated = Invoke-CimMethod -Namespace root/default -ClassName StdRegProv -MethodName EnumValues -Arguments @{hDefKey=[uint32]2147483651; sSubKeyName=$key}
        if ($enumerated.ReturnValue -eq 2) { return @{ReturnValue=2} }
        if ($enumerated.ReturnValue -ne 0) { throw "Registry enumeration failed ($($enumerated.ReturnValue))." }
        $index = -1
        for ($i=0; $i -lt @($enumerated.sNames).Count; $i++) { if ($enumerated.sNames[$i] -ieq $name) { $index=$i; break } }
        if ($index -lt 0) { return @{ReturnValue=2} }
        $expectedType = if ($method -eq 'GetStringValue') { 1 } else { 3 }
        if ($enumerated.Types[$index] -ne $expectedType) { throw "Unexpected registry type for $name; no changes made." }
    }
    $result = Invoke-CimMethod -Namespace root/default -ClassName StdRegProv -MethodName $method -Arguments $arguments
    if ($result.ReturnValue -notin @(0,2)) { throw "Registry $method failed ($($result.ReturnValue))." }
    return $result
}
function Approval($area, $name) {
    $result = RegistryCall GetBinaryValue "$approvalRoot\$area" $name
    if ($result.ReturnValue -eq 2) { return 'Not set' }
    if ($result.uValue.Count -lt 12) { return 'Unknown' }
    $state = [BitConverter]::ToUInt32([byte[]]$result.uValue,0)
    if ($state -in @(2,6)) { return 'Enabled' }
    return "Disabled or unknown ($state)"
}
function StartupEntries {
    @(Get-CimInstance Win32_StartupCommand | Where-Object { $_.UserSID -eq $sid -and $_.Name -eq 'AudioControl' })
}
function IsOwnedCommand([string]$command) {
    return $command -match '^"[^"\r\n]*[\\/]AudioControl\.exe"(?: --portable)?$'
}
$old = RegistryCall GetStringValue $runKey 'AudioControl'
$oldCommand = if ($old.ReturnValue -eq 0) { $old.sValue } else { $null }
$shortcut = $null
if (Test-Path -LiteralPath $shortcutPath) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    if ([IO.Path]::GetFileName($shortcut.TargetPath) -ine 'AudioControl.exe' -or $shortcut.Arguments.Trim() -notin @('', '--portable')) {
        throw 'AudioControl.lnk is not a recognized app-owned shortcut; leaving it untouched.'
    }
}
$runApproval = Approval 'Run' 'AudioControl'
$legacyApproval = Approval 'StartupFolder' 'AudioControl.lnk'
$entries = @(StartupEntries)
if ($Status) {
    $runEntries = @($entries | Where-Object { $_.Location -ieq "HKU\$runKey" })
    $inventoryMatches = if ($null -ne $oldCommand) {
        $runEntries.Count -eq 1 -and $runEntries[0].Command -ceq $oldCommand
    } else { $runEntries.Count -eq 0 }
    if (!$inventoryMatches) { throw 'Run registration and independent Windows startup inventory disagree.' }
    [pscustomobject]@{ User=$interactiveUser; SID=$sid; RunCommand=$oldCommand; WindowsRunVerified=$inventoryMatches; RunApproval=$runApproval; LegacyShortcut=[bool]$shortcut; LegacyApproval=$legacyApproval; WindowsStartupCommands=@($entries | Select-Object Command,Location) }
    return
}
if ($oldCommand -and !(IsOwnedCommand $oldCommand)) { throw 'Run value AudioControl is not a recognized app-owned command; leaving it untouched.' }
if ($Disable) {
    if ($oldCommand) { $null = RegistryCall DeleteValue $runKey 'AudioControl' }
    if ($shortcut) { Remove-Item -LiteralPath $shortcutPath }
    if ((RegistryCall GetStringValue $runKey 'AudioControl').ReturnValue -ne 2 -or (Test-Path -LiteralPath $shortcutPath) -or @(StartupEntries).Count) {
        throw 'Startup removal could not be independently verified.'
    }
    Write-Output 'AudioControl startup registration removed. Windows approval preferences were preserved.'
    return
}
if ($runApproval -notin @('Not set','Enabled') -or ($shortcut -and $legacyApproval -notin @('Not set','Enabled'))) {
    throw "Windows has disabled or unrecognized startup approval (Run: $runApproval; shortcut: $legacyApproval). Enable AudioControl in Windows Startup apps before migrating; no changes made."
}
if (!$ExecutablePath) {
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot '.git') -PathType Leaf) {
        throw 'In a Git worktree, pass -ExecutablePath pointing to the permanent installed build.'
    }
    $ExecutablePath = Join-Path $PSScriptRoot 'build\AudioControl.exe'
}
$executable = (Get-Item -LiteralPath $ExecutablePath).FullName
if ([IO.Path]::GetFileName($executable) -ine 'AudioControl.exe' -or !(Test-Path -LiteralPath $executable -PathType Leaf) -or $executable -match '["\r\n]') { throw 'Select an existing AudioControl.exe file.' }
$usePortable = $Portable -or ($shortcut -and $shortcut.Arguments.Trim() -eq '--portable') -or ($oldCommand -and $oldCommand.EndsWith(' --portable'))
$command = '"' + $executable + '"'
if ($usePortable) { $command += ' --portable' }
if ($command.Length -gt 260) { throw 'The Windows Run command exceeds the 260-character limit.' }
try {
    $null = RegistryCall CreateKey $runKey $null
    $null = RegistryCall SetStringValue $runKey 'AudioControl' @{sValue=$command}
    $saved = RegistryCall GetStringValue $runKey 'AudioControl'
    $verified = @(StartupEntries | Where-Object { $_.Command -ceq $command -and ($_.Location -ieq "HKU\$runKey") })
    if ($saved.sValue -cne $command -or $verified.Count -ne 1) { throw 'Windows Win32_StartupCommand did not independently confirm the Run registration.' }
} catch {
    if ($null -ne $oldCommand) { $null = RegistryCall SetStringValue $runKey 'AudioControl' @{sValue=$oldCommand} }
    else { $null = RegistryCall DeleteValue $runKey 'AudioControl' }
    throw
}
# Only retire the owned shortcut once Windows independently sees the usable replacement.
if ($shortcut) { Remove-Item -LiteralPath $shortcutPath }
Write-Output "AudioControl will launch at sign-in for ${interactiveUser}: $command (Windows verified)."
