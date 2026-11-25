param([string]$Compiler,[switch]$UIAccess)
$ErrorActionPreference = 'Stop'
if(!$Compiler){
    $bundled=Join-Path $PSScriptRoot '..\tools\w64devkit\bin\g++.exe'
    if(Test-Path -LiteralPath $bundled){$Compiler=$bundled}else{$command=Get-Command g++.exe -ErrorAction SilentlyContinue;if($command){$Compiler=$command.Source}}
}
if(!$Compiler -or !(Test-Path -LiteralPath $Compiler)){throw 'MinGW-w64 g++ not found. Put g++.exe on PATH or pass -Compiler.'}
$out = Join-Path $PSScriptRoot 'bin'
if($UIAccess){$out=Join-Path $out 'uiaccess'}
New-Item -ItemType Directory -Force $out | Out-Null
$windres=Join-Path (Split-Path $Compiler -Parent) 'windres.exe'
$buildDefines=@();if($UIAccess){$buildDefines=@('-DCORNER_UIACCESS')}
& $windres @buildDefines (Join-Path $PSScriptRoot 'src\app.rc') -O coff -o (Join-Path $out 'resources.o')
if($LASTEXITCODE){throw 'Resource compilation failed'}
& $Compiler @buildDefines -std=c++17 -O2 -Wall -Wextra -Werror -municode -mwindows -static -static-libgcc -static-libstdc++ (Join-Path $PSScriptRoot 'src\main.cpp') (Join-Path $out 'resources.o') -o (Join-Path $out 'corner.exe') -luser32 -lgdi32 -lgdiplus -ldwmapi -lole32 -lshell32 -luuid -lcomctl32 -ladvapi32 -loleaut32 -ltaskschd
if ($LASTEXITCODE) { throw "Build failed: $LASTEXITCODE" }
Get-Item (Join-Path $out 'corner.exe') | Select-Object FullName,Length
