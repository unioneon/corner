[CmdletBinding()]
param(
    [ValidateSet('Enable','Disable')][string]$Action='Enable',
    [string]$InstallDir=(Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'corner'),
    [switch]$Consent
)
$ErrorActionPreference='Stop'
$InstallDir=[IO.Path]::GetFullPath($InstallDir)
if(!(Test-Path -LiteralPath (Join-Path $InstallDir '.corner-install'))){throw 'Install corner before enabling Enhanced mode.'}
if($Action -eq 'Enable' -and !$Consent) {
    Add-Type -AssemblyName PresentationFramework
    $message='Enhanced mode keeps corners above Start and the taskbar. It requires administrator approval, installs a UIAccess executable in Program Files, and creates a machine-local code-signing certificate trusted by Windows. UIAccess expands access to other applications'' UI. The private key stays on this PC. This is an experimental option; Microsoft reserves UIAccess for accessibility applications. Continue?'
    if([Windows.MessageBox]::Show($message,'corner — Enhanced mode','YesNo','Warning') -ne 'Yes'){exit 1}
}
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$taskName='corner-'+$sid
$marker=Join-Path $InstallDir '.uiaccess-runtime'
$wasEnhanced=Test-Path -LiteralPath $marker
$manager=Join-Path $InstallDir 'manage.ps1'
$report=Join-Path $InstallDir 'enhanced-result.json'
$worker=Join-Path $InstallDir 'enhanced-admin.ps1'
if(!(Test-Path -LiteralPath $worker)){throw 'Enhanced mode files are missing. Reinstall corner.'}
if($Action -eq 'Enable' -and !(Test-Path -LiteralPath (Join-Path $InstallDir 'enhanced\corner.exe'))){throw 'Enhanced executable is missing.'}
& $manager stop -InstallDir $InstallDir
try {
    if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
    $workerAction=if($Action -eq 'Enable'){'Install'}else{'Remove'}
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$worker+'"'),$workerAction,'-InstallDir',('"'+$InstallDir+'"'),'-OwnerSid',$sid)
    $process=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -WindowStyle Hidden -ArgumentList $arguments -PassThru -Wait
    if($process.ExitCode -or !(Test-Path -LiteralPath $report)){throw 'Enhanced mode setup did not complete. Administrator approval may have been cancelled.'}
    $result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
    if(!$result.Success){throw $result.Error}
    if($Action -eq 'Enable'){Set-Content -LiteralPath $marker -Value 'corner-local-uiaccess-v1'}
    elseif(Test-Path -LiteralPath $marker){Remove-Item -LiteralPath $marker}
    $exe=Join-Path $InstallDir 'corner.exe'
    $args=if($Action -eq 'Enable'){'--supervise-uiaccess --rounded --external'}else{'--supervise --rounded --external'}
    Set-ScheduledTask -TaskName $taskName -Action (New-ScheduledTaskAction -Execute $exe -Argument $args -WorkingDirectory $InstallDir)|Out-Null
} catch {
    # Keep the previous mode if elevation was cancelled or deployment failed.
    $protectedAvailable=Test-Path -LiteralPath (Join-Path $env:ProgramFiles 'corner\corner.exe')
    if($wasEnhanced -and $protectedAvailable -and !(Test-Path -LiteralPath $marker)){Set-Content -LiteralPath $marker -Value 'corner-local-uiaccess-v1'}
    if(!$protectedAvailable) {
        if(Test-Path -LiteralPath $marker){Remove-Item -LiteralPath $marker}
        Set-ScheduledTask -TaskName $taskName -Action (New-ScheduledTaskAction -Execute (Join-Path $InstallDir 'corner.exe') -Argument '--supervise --rounded --external' -WorkingDirectory $InstallDir)|Out-Null
    }
    & $manager restart -InstallDir $InstallDir
    Add-Type -AssemblyName PresentationFramework
    [Windows.MessageBox]::Show($_.Exception.Message,'corner — Setup did not complete','OK','Error')|Out-Null
    throw
}
& $manager restart -InstallDir $InstallDir
Write-Output ('Enhanced mode '+$Action.ToLowerInvariant()+'d.')
