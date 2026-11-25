param([string]$Executable=(Join-Path $PSScriptRoot '..\bin\corner.exe'))
$ErrorActionPreference='Stop'
$exe=(Resolve-Path -LiteralPath $Executable).Path
$temp=Join-Path ([IO.Path]::GetTempPath()) ('corner-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp|Out-Null
try {
    $result=Join-Path $temp 'self-test.json'
    $process=Start-Process -FilePath $exe -ArgumentList @('--self-test','--report',('"'+$result+'"')) -WindowStyle Hidden -PassThru -Wait
    if($process.ExitCode){throw (Get-Content -LiteralPath $result -Raw)}
    $data=Get-Content -LiteralPath $result -Raw|ConvertFrom-Json
    foreach($property in $data.PSObject.Properties){if($property.Name -ne 'physicalContourMatch' -and $property.Value -ne 'pass'){throw "Failed: $($property.Name)"}}
    $captures=Join-Path $temp 'captures';$captureReport=Join-Path $temp 'capture.json'
    $process=Start-Process -FilePath $exe -ArgumentList @('--export-ui',('"'+$captures+'"'),'--report',('"'+$captureReport+'"')) -WindowStyle Hidden -PassThru -Wait
    if($process.ExitCode){throw 'Native UI capture failed'}
    $capture=Get-Content -LiteralPath $captureReport -Raw|ConvertFrom-Json
    if($capture.antialiasedEdgePixels -le 0){throw 'The UI contour has no antialiasing'}
    foreach($name in 'settings-light.png','settings-dark.png'){if((Get-Item -LiteralPath (Join-Path $captures $name)).Length -lt 1000){throw 'Empty screenshot'}}
    [pscustomobject]@{SelfTest='pass';NativeUiCapture='pass';AntialiasedPixels=$capture.antialiasedEdgePixels;Font=$capture.font}|ConvertTo-Json
}finally{
    # Delete only the known files created by this test, never recurse.
    foreach($name in @('self-test.json','capture.json','captures\settings-light.png','captures\settings-dark.png')){$file=Join-Path $temp $name;if(Test-Path -LiteralPath $file){Remove-Item -LiteralPath $file}}
    $dir=Join-Path $temp 'captures';if((Test-Path -LiteralPath $dir) -and !(Get-ChildItem -LiteralPath $dir -Force)){Remove-Item -LiteralPath $dir}
    if(!(Get-ChildItem -LiteralPath $temp -Force)){Remove-Item -LiteralPath $temp}
}
