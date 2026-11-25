param([string]$InstallerCompiler,[string]$Version=(Get-Content (Join-Path $PSScriptRoot '..\VERSION') -Raw).Trim())
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent));$exe=Join-Path $root 'bin\corner.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Build bin/corner.exe first.'}
if((Get-Item -LiteralPath $exe).VersionInfo.ProductVersion -ne $Version){throw 'Executable and release versions differ.'}
if(!$InstallerCompiler){$InstallerCompiler=Join-Path $root '..\tools\InnoSetup7\ISCC.exe'}
if(!(Test-Path -LiteralPath $InstallerCompiler)){throw 'Pass -InstallerCompiler with the path to Inno Setup ISCC.exe.'}
$dist=Join-Path $root 'dist';$stage=Join-Path $dist ('corner-'+$Version+'-portable')
New-Item -ItemType Directory -Path $dist -Force|Out-Null
if(Test-Path -LiteralPath $stage){
    $allowed=@('manage.ps1','Settings.cmd','Install.cmd','Uninstall.cmd','LICENSE','THIRD_PARTY_NOTICES.md','corner.exe','MinGW-RUNTIME.txt','README.txt')
    foreach($entry in Get-ChildItem -LiteralPath $stage -Force){if($entry.PSIsContainer -or $entry.Name -notin $allowed){throw 'Unexpected portable staging content. Use a fresh folder.'}}
}else{New-Item -ItemType Directory -Path $stage|Out-Null}
foreach($file in @('manage.ps1','Settings.cmd','Install.cmd','Uninstall.cmd','LICENSE','THIRD_PARTY_NOTICES.md')){Copy-Item -LiteralPath (Join-Path $root $file) -Destination $stage -Force}
Copy-Item -LiteralPath $exe -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $root 'docs\MinGW-RUNTIME.txt') -Destination $stage -Force
@'
corner - A softer edge to Windows.

Extract this entire folder and run corner.exe. Keep it in a writable folder.
To install for your current account, run Install.cmd.

Up / Down: toggle the upper / lower pair. Sliders preview immediately.
Apply: save. OK: close and return to the last Apply.
Click the logo for mask color and appearance. Start Windows controls sign-in.

https://github.com/unioneon/corner
'@ | Set-Content -LiteralPath (Join-Path $stage 'README.txt') -Encoding UTF8
& (Join-Path $root 'tests\smoke.ps1') -Executable (Join-Path $stage 'corner.exe')
$zip=Join-Path $dist "corner-$Version-Portable-x64.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
& $InstallerCompiler /Qp "/DAppVersion=$Version" (Join-Path $root 'installer\corner.iss')
if($LASTEXITCODE){throw 'Installer compilation failed.'}
$source=Join-Path $dist "corner-$Version-Source.zip"
& git -C $root archive --format=zip --output=$source HEAD
if($LASTEXITCODE){throw 'Source archive failed. Commit the prepared source before packaging.'}
$lines=Get-ChildItem -LiteralPath $dist -File|Where-Object {$_.Name.StartsWith("corner-$Version-") -and $_.Name -match '\.(exe|zip)$'}|Sort-Object Name|ForEach-Object {'{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),$_.Name}
$lines|Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt') -Encoding ASCII
Get-ChildItem -LiteralPath $dist -File|Select-Object Name,Length
