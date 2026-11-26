[CmdletBinding()]
param(
    [Parameter(Position=0)][ValidateSet('install','uninstall','status','restart','stop','debug','list','settings')][string]$Action='status',
    [string]$LeftMask,[string]$RightMask,[string]$Panel,[int]$Width,[int]$Height,
    [switch]$Demo,[switch]$Rounded,[ValidateRange(8,256)][int]$Radius=40,
    [switch]$DebugMode,[switch]$AllowExternal,[switch]$NoPrivateApi,[switch]$NoDesktopShortcut,
    [string]$InstallDir=(Join-Path $env:LOCALAPPDATA 'corner')
)
$ErrorActionPreference='Stop'
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$taskName='corner-'+$sid;$legacyTaskName='GlassCorners-'+$sid
$sourceExe=Join-Path $PSScriptRoot 'bin\corner.exe'
if(!(Test-Path -LiteralPath $sourceExe)){$sourceExe=Join-Path $PSScriptRoot 'corner.exe'}
$installedExe=Join-Path $InstallDir 'corner.exe'
$uiAccessExe=Join-Path $env:ProgramFiles 'corner\corner.exe'
$uiAccessMarker=Join-Path $InstallDir '.uiaccess-runtime'
$legacyDir=Join-Path $env:LOCALAPPDATA 'GlassCorners'
$legacyExe=Join-Path $legacyDir 'GlassCorners.exe'
if(!$Demo -and !$LeftMask -and !$RightMask){$Rounded=$true}
if($Rounded){$AllowExternal=$true}
function Quote-Arg([string]$Text){'"'+([regex]::Replace(([regex]::Replace($Text,'(\\*)"','$1$1\"')),'(\\+)$','$1$1'))+'"'}
function Invoke-Corner([string]$Executable,[string[]]$Arguments,[switch]$ReadJson) {
    if(!(Test-Path -LiteralPath $Executable)){throw "Executable missing: $Executable. Run build.ps1."}
    $items=@($Arguments);$temp=$null
    if($ReadJson){$temp=[IO.Path]::GetTempFileName();$items+=@('--report',$temp)}
    try {
        $info=New-Object Diagnostics.ProcessStartInfo;$info.FileName=$Executable
        $info.Arguments=($items|ForEach-Object {Quote-Arg $_}) -join ' ';$info.UseShellExecute=($Executable -eq $uiAccessExe -and (Test-Path -LiteralPath $uiAccessMarker));$info.CreateNoWindow=$true;$info.WindowStyle='Hidden'
        $process=[Diagnostics.Process]::Start($info)
        if(!$process.WaitForExit(10000)){$process.Kill();throw 'corner command timed out'}
        $data=$null;if($ReadJson){$data=Get-Content -LiteralPath $temp -Raw | ConvertFrom-Json}
        if($process.ExitCode){throw "corner failed ($($process.ExitCode)): $($data.error)"}
        if($ReadJson){$data}
    }finally{if($temp -and (Test-Path -LiteralPath $temp)){Remove-Item -LiteralPath $temp}}
}
function Runtime-Exe {if((Test-Path -LiteralPath $uiAccessMarker) -and (Test-Path -LiteralPath $uiAccessExe)){$uiAccessExe}elseif(Test-Path -LiteralPath $installedExe){$installedExe}else{$sourceExe}}
function Stop-Corner {
    $task=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    if($task){Disable-ScheduledTask -TaskName $taskName | Out-Null}
    $exe=Runtime-Exe;if(Test-Path -LiteralPath $exe){Invoke-Corner $exe @('--stop')}
    if($task){Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue}
    for($i=0;$i -lt 50;$i++) {
        $live=@(Get-Process -Name corner,GlassCorners -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $installedExe -or $_.Path -eq $sourceExe -or $_.Path -eq $legacyExe})
        if(!$live.Count){return};Start-Sleep -Milliseconds 100
    }
    throw 'corner did not stop. No executable was replaced.'
}
function Make-Shortcut([string]$Path) {
    $shell=New-Object -ComObject WScript.Shell;$link=$shell.CreateShortcut($Path)
    $link.TargetPath=$installedExe;$link.Arguments='--settings';$link.WorkingDirectory=$InstallDir;$link.IconLocation=$installedExe+',0';$link.Description='corner - screen corners';$link.Save()
    [Runtime.InteropServices.Marshal]::ReleaseComObject($link) | Out-Null
    [Runtime.InteropServices.Marshal]::ReleaseComObject($shell) | Out-Null
}
function Wants-Startup {
    $file=Join-Path $InstallDir 'settings.ini'
    !((Test-Path -LiteralPath $file) -and (Select-String -LiteralPath $file -Pattern '^Startup=0\s*$' -Quiet))
}
switch($Action) {
    'settings' {Start-Process -FilePath (Runtime-Exe) -ArgumentList '--settings' -WindowStyle Normal}
    'list' {Invoke-Corner $sourceExe @('--list') -ReadJson | ConvertTo-Json -Depth 5}
    'status' {
        $runtime=Invoke-Corner (Runtime-Exe) @('--status') -ReadJson
        $task=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
        [pscustomobject]@{Product='corner';InstallDir=$InstallDir;TaskRegistered=[bool]$task;TaskState=if($task){[string]$task.State}else{'Absent'};Runtime=$runtime}|ConvertTo-Json -Depth 5
    }
    'stop' {Stop-Corner}
    'restart' {
        Get-ScheduledTask -TaskName $taskName -ErrorAction Stop | Out-Null
        $startup=Wants-Startup;Stop-Corner;Enable-ScheduledTask -TaskName $taskName | Out-Null;Start-ScheduledTask -TaskName $taskName
        if(!$startup){Disable-ScheduledTask -TaskName $taskName|Out-Null}
    }
    'debug' {
        $items=@('--supervise','--debug')
        if($Demo){$items+='--demo'}elseif($Rounded){$items+=@('--rounded','--radius',[string]$Radius)}else{
            if(!$LeftMask -or !$RightMask -or !$Width -or !$Height){throw 'Supply masks and physical resolution, or use -Rounded.'}
            $items+=@('--left',(Resolve-Path -LiteralPath $LeftMask).Path,'--right',(Resolve-Path -LiteralPath $RightMask).Path,'--width',[string]$Width,'--height',[string]$Height)
        }
        if($Panel){$items+=@('--panel',$Panel)};if($AllowExternal){$items+='--external'};if($NoPrivateApi){$items+='--no-private-api'}
        Start-Process -FilePath $sourceExe -ArgumentList (($items|ForEach-Object {Quote-Arg $_}) -join ' ') -WindowStyle Hidden
    }
    'install' {
        if(!(Test-Path -LiteralPath $sourceExe)){throw 'Run build.ps1 first.'}
        if($Demo -and !$DebugMode){throw '-Demo requires -DebugMode.'}
        $topology=Invoke-Corner $sourceExe @('--list') -ReadJson
        $candidates=@($topology.panels|Where-Object {(!$Panel -and $_.internal) -or ($Panel -and $_.path -eq $Panel -and ($_.internal -or $AllowExternal))})
        if($Rounded -and !$Panel){$candidates=@($topology.panels)}
        if(!$Demo -and (!$candidates.Count -or (!$Rounded -and $candidates.Count -ne 1))){throw 'No matching display. Nothing was installed.'}
        if(!$Demo -and !$Rounded) {
            if(!$LeftMask -or !$RightMask){throw 'Mask files are required.'}
            if($Width -ne $candidates[0].width -or $Height -ne $candidates[0].height){throw 'Mask resolution does not match.'}
            $LeftMask=(Resolve-Path -LiteralPath $LeftMask).Path;$RightMask=(Resolve-Path -LiteralPath $RightMask).Path
        }
        $validation=@('--validate')
        if($Demo){$validation+=@('--debug','--demo')}elseif($Rounded){$validation+=@('--rounded','--radius',[string]$Radius);if($Panel){$validation+=@('--panel',$Panel)}}else{
            $validation+=@('--left',$LeftMask,'--right',$RightMask,'--panel',$candidates[0].path,'--width',[string]$Width,'--height',[string]$Height)
        }
        if($AllowExternal){$validation+='--external'};Invoke-Corner $sourceExe $validation -ReadJson|Out-Null
        $absolute=[IO.Path]::GetFullPath($InstallDir).TrimEnd('\');$marker=Join-Path $absolute '.corner-install'
        if($absolute -eq [IO.Path]::GetPathRoot($absolute).TrimEnd('\')){throw 'Refusing a drive root.'}
        if((Test-Path -LiteralPath $absolute) -and !(Test-Path -LiteralPath $marker)){throw 'Directory exists without a corner ownership marker.'}
        $legacyTask=Get-ScheduledTask -TaskName $legacyTaskName -ErrorAction SilentlyContinue
        if($legacyTask){Disable-ScheduledTask -TaskName $legacyTaskName|Out-Null;Stop-ScheduledTask -TaskName $legacyTaskName -ErrorAction SilentlyContinue}
        Stop-Corner
        New-Item -ItemType Directory -Path $absolute -Force|Out-Null
        Set-Content -LiteralPath $marker -Value 'corner per-user install v1' -Encoding UTF8
        $oldSettings=Join-Path $legacyDir 'settings.ini';$newSettings=Join-Path $absolute 'settings.ini'
        if(!(Test-Path -LiteralPath $newSettings) -and (Test-Path -LiteralPath $oldSettings)){Copy-Item -LiteralPath $oldSettings -Destination $newSettings}
        if([IO.Path]::GetFullPath($sourceExe) -ne [IO.Path]::GetFullPath($installedExe)){Copy-Item -LiteralPath $sourceExe -Destination $installedExe -Force}
        $manager=Join-Path $absolute 'manage.ps1';if([IO.Path]::GetFullPath($PSCommandPath) -ne [IO.Path]::GetFullPath($manager)){Copy-Item -LiteralPath $PSCommandPath -Destination $manager -Force}
        foreach($name in @('enhanced.ps1','enhanced-admin.ps1')) {
            $from=Join-Path $PSScriptRoot $name;$to=Join-Path $absolute $name
            if((Test-Path -LiteralPath $from) -and [IO.Path]::GetFullPath($from) -ne [IO.Path]::GetFullPath($to)){Copy-Item -LiteralPath $from -Destination $to -Force}
        }
        $payload=Join-Path $PSScriptRoot 'enhanced\corner.exe';if(!(Test-Path -LiteralPath $payload)){$payload=Join-Path $PSScriptRoot 'bin\uiaccess\corner.exe'}
        $payloadTarget=Join-Path $absolute 'enhanced\corner.exe'
        if((Test-Path -LiteralPath $payload) -and [IO.Path]::GetFullPath($payload) -ne [IO.Path]::GetFullPath($payloadTarget)) {
            New-Item -ItemType Directory -Path (Split-Path $payloadTarget -Parent) -Force|Out-Null
            Copy-Item -LiteralPath $payload -Destination $payloadTarget -Force
        }
        $items=@('--supervise');if($DebugMode){$items+='--debug'}
        if($Demo){$items+='--demo'}elseif($Rounded){$items+=@('--rounded','--radius',[string]$Radius);if($Panel){$items+=@('--panel',$Panel)}}else{
            Copy-Item -LiteralPath $LeftMask -Destination (Join-Path $absolute 'left.png') -Force;Copy-Item -LiteralPath $RightMask -Destination (Join-Path $absolute 'right.png') -Force
            $items+=@('--left',(Join-Path $absolute 'left.png'),'--right',(Join-Path $absolute 'right.png'),'--panel',$candidates[0].path,'--width',[string]$Width,'--height',[string]$Height)
        }
        if($AllowExternal){$items+='--external'};if($NoPrivateApi){$items+='--no-private-api'}
        $principal=New-ScheduledTaskPrincipal -UserId $sid -LogonType Interactive -RunLevel Limited
        $trigger=New-ScheduledTaskTrigger -AtLogOn -User $sid
        $taskSettings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1) -MultipleInstances IgnoreNew -StartWhenAvailable
        $taskAction=New-ScheduledTaskAction -Execute $installedExe -Argument (($items|ForEach-Object {Quote-Arg $_}) -join ' ') -WorkingDirectory $absolute
        Register-ScheduledTask -TaskName $taskName -Action $taskAction -Trigger $trigger -Principal $principal -Settings $taskSettings -Description 'corner - screen corners' -Force|Out-Null
        Enable-ScheduledTask -TaskName $taskName|Out-Null;Start-ScheduledTask -TaskName $taskName
        $live=$null
        for($attempt=0;$attempt -lt 24;$attempt++){Start-Sleep -Milliseconds 250;$live=Invoke-Corner $installedExe @('--status') -ReadJson;if($live.running -and $live.renders -ge 2){break}}
        if(!$live.running -or $live.renders -lt 2){throw 'corner renderer did not start.'}
        if(!(Wants-Startup)){Disable-ScheduledTask -TaskName $taskName|Out-Null}
        Make-Shortcut (Join-Path ([Environment]::GetFolderPath('Programs')) 'corner.lnk')
        if(!$NoDesktopShortcut){Make-Shortcut (Join-Path ([Environment]::GetFolderPath('Desktop')) 'corner.lnk')}
        if($legacyTask){Unregister-ScheduledTask -TaskName $legacyTaskName -Confirm:$false}
        $oldShell=New-Object -ComObject WScript.Shell
        foreach($oldLink in Get-ChildItem -LiteralPath ([Environment]::GetFolderPath('Programs')) -Filter 'GlassCorners*.lnk' -ErrorAction SilentlyContinue){
            $oldShortcut=$oldShell.CreateShortcut($oldLink.FullName)
            if($oldShortcut.TargetPath -ieq $legacyExe){Remove-Item -LiteralPath $oldLink.FullName -Force}
            [Runtime.InteropServices.Marshal]::ReleaseComObject($oldShortcut)|Out-Null
        }
        [Runtime.InteropServices.Marshal]::ReleaseComObject($oldShell)|Out-Null
        if(Test-Path -LiteralPath $legacyExe){try{Invoke-Corner $legacyExe @('--unpin-helper')}catch{Write-Warning $_}}
        [pscustomobject]@{Installed=$true;Product='corner';InstallDir=$absolute;Runtime=$live}|ConvertTo-Json -Depth 5
    }
    'uninstall' {
        if(Test-Path -LiteralPath $uiAccessMarker){& (Join-Path $InstallDir 'enhanced.ps1') Disable -Consent -InstallDir $InstallDir}
        $absolute=[IO.Path]::GetFullPath($InstallDir).TrimEnd('\');$marker=Join-Path $absolute '.corner-install'
        if(!(Test-Path -LiteralPath $marker) -or (Get-Content -LiteralPath $marker -Raw).Trim() -ne 'corner per-user install v1'){throw 'Invalid ownership marker. No files removed.'}
        Stop-Corner;$task=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
        if($task){Unregister-ScheduledTask -TaskName $taskName -Confirm:$false}
        try{Invoke-Corner $installedExe @('--unpin-helper')}catch{Write-Warning $_}
        foreach($folder in @([Environment]::GetFolderPath('Programs'),[Environment]::GetFolderPath('Desktop'))){$link=Join-Path $folder 'corner.lnk';if(Test-Path -LiteralPath $link){Remove-Item -LiteralPath $link -Force}}
        foreach($name in @('corner.exe','manage.ps1','settings.ini','settings.ini.tmp','left.png','right.png','.corner-install','enhanced.ps1','enhanced-admin.ps1','enhanced-result.json','enhanced\corner.exe')){$target=Join-Path $absolute $name;if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target -Force}}
        $payloadFolder=Join-Path $absolute 'enhanced';if((Test-Path -LiteralPath $payloadFolder) -and !(Get-ChildItem -LiteralPath $payloadFolder -Force)){Remove-Item -LiteralPath $payloadFolder}
        if(!(Get-ChildItem -LiteralPath $absolute -Force)){Remove-Item -LiteralPath $absolute}
        'corner uninstalled.'
    }
}
