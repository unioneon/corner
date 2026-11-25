# Elevated worker for Enhanced mode. Called only after explicit opt-in.
[CmdletBinding()]
param([ValidateSet('Install','Remove')][string]$Action='Install',[Parameter(Mandatory=$true)][string]$InstallDir,[Parameter(Mandatory=$true)][string]$OwnerSid)
$ErrorActionPreference='Stop'
$InstallDir=[IO.Path]::GetFullPath($InstallDir)
if(!(Test-Path -LiteralPath (Join-Path $InstallDir '.corner-install'))){throw 'Install corner for this user first.'}
$target=Join-Path $env:ProgramFiles 'corner'
$marker=Join-Path $target '.corner-uiaccess.json'
$report=Join-Path $InstallDir 'enhanced-result.json'
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=New-Object Security.Principal.WindowsPrincipal $identity
if(!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Run this deployment script as administrator.'}
try {
    if($Action -eq 'Remove') {
        if(!(Test-Path -LiteralPath $marker)){throw 'No owned UIAccess installation found.'}
        $state=Get-Content -LiteralPath $marker -Raw|ConvertFrom-Json
        if($state.Product -ne 'corner-local-uiaccess-v1'){throw 'Invalid ownership marker.'}
        if($state.OwnerSid -and $state.OwnerSid -ne $OwnerSid){throw 'Enhanced mode is owned by a different Windows user.'}
        $resolved=[IO.Path]::GetFullPath($target)
        if($resolved -ne (Join-Path $env:ProgramFiles 'corner')){throw 'Unexpected removal target.'}
        # Only explicit files owned by this installer; never recursively delete.
        foreach($file in @('corner.exe','fonts\SF-Pro-Display-Regular.otf','fonts\SF-Pro-Display-Bold.otf','.corner-uiaccess.json')) {
            $path=Join-Path $target $file;if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}
        }
        foreach($folder in @((Join-Path $target 'fonts'),$target)) {
            if((Test-Path -LiteralPath $folder) -and !(Get-ChildItem -LiteralPath $folder -Force)){Remove-Item -LiteralPath $folder}
        }
        foreach($store in @('Root','TrustedPublisher','My')) {
            $path='Cert:\LocalMachine\'+$store+'\'+$state.Thumbprint
            if(Test-Path -LiteralPath $path){
                if($store -eq 'My'){Remove-Item -LiteralPath $path -DeleteKey}else{Remove-Item -LiteralPath $path}
            }
        }
        @{Success=$true;Action=$Action}|ConvertTo-Json|Set-Content -LiteralPath $report
        exit 0
    }
    $source=Join-Path $InstallDir 'enhanced\corner.exe'
    if(!(Test-Path -LiteralPath $source)){throw 'Build the experimental UIAccess executable first.'}
    if((Test-Path -LiteralPath $target) -and !(Test-Path -LiteralPath $marker)){throw 'Target exists without a corner ownership marker.'}
    New-Item -ItemType Directory -Path $target -Force|Out-Null
    $certificate=$null
    if(Test-Path -LiteralPath $marker) {
        $previous=Get-Content -LiteralPath $marker -Raw|ConvertFrom-Json
        if($previous.Product -ne 'corner-local-uiaccess-v1'){throw 'Invalid ownership marker.'}
        if($previous.OwnerSid -and $previous.OwnerSid -ne $OwnerSid){throw 'Enhanced mode is owned by a different Windows user.'}
        $certificate=Get-Item -LiteralPath ('Cert:\LocalMachine\My\'+$previous.Thumbprint)
    } else {
        $certificate=New-SelfSignedCertificate -Type CodeSigningCert -Subject ('CN=corner Local UIAccess '+[guid]::NewGuid().ToString('N')) -CertStoreLocation Cert:\LocalMachine\My -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 -KeyExportPolicy NonExportable -NotAfter (Get-Date).AddYears(1)
        @{Product='corner-local-uiaccess-v1';Thumbprint=$certificate.Thumbprint;OwnerSid=$OwnerSid}|ConvertTo-Json|Set-Content -LiteralPath $marker
    }
    @{Product='corner-local-uiaccess-v1';Thumbprint=$certificate.Thumbprint;OwnerSid=$OwnerSid}|ConvertTo-Json|Set-Content -LiteralPath $marker
    # Trust this explicitly approved local code-signing certificate, not a CA.
    foreach($name in @('Root','TrustedPublisher')) {
        $store=New-Object Security.Cryptography.X509Certificates.X509Store $name,'LocalMachine'
        try{$store.Open('ReadWrite');$store.Add($certificate)}finally{$store.Close()}
    }
    $binary=Join-Path $target 'corner.exe'
    $staged=Join-Path $target 'corner.next.exe'
    Copy-Item -LiteralPath $source -Destination $staged -Force
    $signature=Set-AuthenticodeSignature -FilePath $staged -Certificate $certificate -HashAlgorithm SHA256
    if($signature.Status -ne 'Valid'){throw ('Signature verification failed: '+$signature.StatusMessage)}
    Move-Item -LiteralPath $staged -Destination $binary -Force
    @{Success=$true;Action=$Action;Executable=$binary;Thumbprint=$certificate.Thumbprint;Signature=[string]$signature.Status;SHA256=(Get-FileHash -LiteralPath $binary).Hash}|ConvertTo-Json|Set-Content -LiteralPath $report
} catch {
    @{Success=$false;Action=$Action;Error=$_.Exception.Message}|ConvertTo-Json|Set-Content -LiteralPath $report
    throw
}
