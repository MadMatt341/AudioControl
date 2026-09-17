$ErrorActionPreference = 'Stop'
$scriptPath = Join-Path $PSScriptRoot 'startup.ps1'
$fixture = Join-Path $PSScriptRoot 'build\startup-tests\AudioControl.exe'
New-Item -ItemType Directory -Force (Split-Path $fixture) | Out-Null
Set-Content -LiteralPath $fixture -Value 'fixture; never executed'
$global:AudioStartupTestValues = @{}
$global:AudioStartupTestShortcut = $null
$global:AudioStartupTestLongPath = $null
$global:AudioStartupTestVisible = $true
$global:AudioStartupTestApproval = $null
$global:AudioStartupTestMismatch = $false
function Get-CimInstance($ClassName,$Filter) {
    $who = [Security.Principal.WindowsIdentity]::GetCurrent()
    if ($ClassName -eq 'Win32_Process') {
        if (!$global:AudioStartupTestNoExplorer) { return [pscustomobject]@{SessionId=(Get-Process -Id $PID).SessionId} }
        return
    }
    if ($global:AudioStartupTestVisible -and $global:AudioStartupTestValues.ContainsKey('AudioControl')) {
        [pscustomobject]@{UserSID=$who.User.Value; Name='AudioControl'; Command=$global:AudioStartupTestValues.AudioControl; Location=("HKU\"+$who.User.Value+"\Software\Microsoft\Windows\CurrentVersion\Run")}
    }
}
function Invoke-CimMethod($Namespace,$ClassName,$MethodName,$Arguments,$InputObject) {
    if ($MethodName -eq 'GetOwnerSid') {
        return @{ReturnValue=0;Sid=$(if ($global:AudioStartupTestMismatch) { 'S-1-5-18' } else { [Security.Principal.WindowsIdentity]::GetCurrent().User.Value })}
    }
    if ($Arguments.hDefKey -ne [uint32]2147483651 -or !$Arguments.sSubKeyName.StartsWith([Security.Principal.WindowsIdentity]::GetCurrent().User.Value+'\')) { throw 'Wrong registry user/hive' }
    switch ($MethodName) {
        EnumValues {
            if ($Arguments.sSubKeyName -like '*StartupApproved*') {
                if ($global:AudioStartupTestApproval) { return @{ReturnValue=0;sNames=@('AudioControl','AudioControl.lnk');Types=@(3,3)} }
                return @{ReturnValue=2}
            }
            return @{ReturnValue=0;sNames=[string[]]@($global:AudioStartupTestValues.Keys);Types=@(1)}
        }
        GetBinaryValue { if ($global:AudioStartupTestApproval) { return @{ReturnValue=0;uValue=$global:AudioStartupTestApproval} }; return @{ReturnValue=2} }
        GetStringValue { if ($global:AudioStartupTestValues.ContainsKey($Arguments.sValueName)) { return @{ReturnValue=0;sValue=$global:AudioStartupTestValues[$Arguments.sValueName]} }; return @{ReturnValue=2} }
        SetStringValue { $global:AudioStartupTestValues[$Arguments.sValueName]=$Arguments.sValue }
        DeleteValue { $global:AudioStartupTestValues.Remove($Arguments.sValueName) }
        CreateKey { }
        default { throw "Unexpected method $MethodName" }
    }
    return @{ReturnValue=0}
}
function Get-Item($LiteralPath) {
    if ($global:AudioStartupTestLongPath) { return [pscustomobject]@{FullName=$global:AudioStartupTestLongPath} }
    Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath
}
function Test-Path {
    param($LiteralPath,$PathType)
    if ($global:AudioStartupTestLongPath -and $LiteralPath -eq $global:AudioStartupTestLongPath) { return $true }
    if ($LiteralPath -like '*AudioControl.lnk') { return [bool]$global:AudioStartupTestShortcut }
    if ($PathType) { return Microsoft.PowerShell.Management\Test-Path -LiteralPath $LiteralPath -PathType $PathType }
    return Microsoft.PowerShell.Management\Test-Path -LiteralPath $LiteralPath
}
function New-Object($ComObject) {
    if ($ComObject -ne 'WScript.Shell') { throw 'Unexpected COM request' }
    $shell = [pscustomobject]@{}
    $shell | Add-Member ScriptMethod CreateShortcut { param($path) return $global:AudioStartupTestShortcut }
    return $shell
}
function Remove-Item($LiteralPath) {
    if ($LiteralPath -notlike '*AudioControl.lnk') { throw 'Unexpected deletion' }
    if (!$global:AudioStartupTestVisible -or !$global:AudioStartupTestValues.ContainsKey('AudioControl')) { throw 'Shortcut removed before verified replacement' }
    $global:AudioStartupTestShortcut=$null
}
function Assert($condition,$message) { if (!$condition) { throw $message } }
function MustFail([scriptblock]$action,[string]$pattern) {
    try { & $action; throw 'Expected failure did not occur' } catch { if ($_.Exception.Message -notmatch $pattern) { throw } }
}
& $scriptPath -ExecutablePath $fixture
Assert ($global:AudioStartupTestValues.AudioControl -ceq ('"'+$fixture+'"')) 'Quoted absolute path missing'
& $scriptPath -ExecutablePath $fixture -Portable
Assert ($global:AudioStartupTestValues.AudioControl.EndsWith(' --portable')) 'Portable flag missing'
& $scriptPath -ExecutablePath $fixture
Assert ($global:AudioStartupTestValues.AudioControl.EndsWith(' --portable')) 'Portable flag lost'
$before = $global:AudioStartupTestValues.AudioControl
$global:AudioStartupTestLongPath='C:\' + ('a' * 250) + '\AudioControl.exe'
MustFail { & $scriptPath -ExecutablePath $fixture } '260-character'
Assert ($global:AudioStartupTestValues.AudioControl -ceq $before) 'Overlong command changed registration'
$global:AudioStartupTestLongPath=$null
$global:AudioStartupTestVisible=$false
MustFail { & $scriptPath -ExecutablePath $fixture } 'independently confirm'
Assert ($global:AudioStartupTestValues.AudioControl -ceq $before) 'Rollback failed'
$global:AudioStartupTestVisible=$true
$global:AudioStartupTestApproval=[byte[]]@(3,0,0,0,0,0,0,0,0,0,0,0)
MustFail { & $scriptPath -ExecutablePath $fixture } 'Windows has disabled'
Assert ($global:AudioStartupTestValues.AudioControl -ceq $before) 'Disabled entry modified'
$global:AudioStartupTestApproval=$null
$global:AudioStartupTestMismatch=$true
MustFail { & $scriptPath -ExecutablePath $fixture } 'interactive user differ'
$global:AudioStartupTestMismatch=$false
$global:AudioStartupTestNoExplorer=$true
MustFail { & $scriptPath -Status } 'desktop session'
$global:AudioStartupTestNoExplorer=$false
$global:AudioStartupTestVisible=$false
MustFail { & $scriptPath -Status } 'inventory disagree'
$global:AudioStartupTestVisible=$true
$status = & $scriptPath -Status
Assert ($status.WindowsRunVerified) 'Independent status verification missing'
Assert ($status.RunCommand -ceq $before) 'Status incorrect'
MustFail { & $scriptPath -Disable -Status } 'parameter set'
& $scriptPath -Disable
Assert (!$global:AudioStartupTestValues.ContainsKey('AudioControl')) 'Disable failed'
$global:AudioStartupTestShortcut=[pscustomobject]@{TargetPath=$fixture;Arguments='--portable'}
$global:AudioStartupTestVisible=$false
MustFail { & $scriptPath -ExecutablePath $fixture } 'independently confirm'
Assert ([bool]$global:AudioStartupTestShortcut) 'Legacy shortcut lost on verification failure'
Assert (!$global:AudioStartupTestValues.ContainsKey('AudioControl')) 'Failed new registration not rolled back'
$global:AudioStartupTestVisible=$true
$global:AudioStartupTestApproval=[byte[]]@(3,0,0,0,0,0,0,0,0,0,0,0)
MustFail { & $scriptPath -ExecutablePath $fixture } 'Windows has disabled'
Assert ([bool]$global:AudioStartupTestShortcut) 'Disabled shortcut removed'
$global:AudioStartupTestApproval=$null
& $scriptPath -ExecutablePath $fixture
Assert (!$global:AudioStartupTestShortcut) 'Legacy shortcut not retired'
Assert ($global:AudioStartupTestValues.AudioControl.EndsWith(' --portable')) 'Legacy portable argument lost'
Write-Output 'Startup fixture tests passed (no live registry writes or shortcut changes).'

Remove-Variable -Scope Global -Name AudioStartupTest*
