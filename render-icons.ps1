$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
function Read-Number($Tokens,[ref]$Index) {
    $v=[single]::Parse($Tokens[$Index.Value].Value,[Globalization.CultureInfo]::InvariantCulture)
    $Index.Value++;return $v
}
function Svg-Path([string]$Data) {
    $tokens=[regex]::Matches($Data,'[MLCVZ]|[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?')
    $path=New-Object Drawing.Drawing2D.GraphicsPath
    $i=0;$x=[single]0;$y=[single]0;$startX=[single]0;$startY=[single]0;$command=''
    while($i -lt $tokens.Count) {
        if($tokens[$i].Value -match '^[MLCVZ]$'){$command=$tokens[$i].Value;$i++}
        switch($command) {
            'M' {$x=Read-Number $tokens ([ref]$i);$y=Read-Number $tokens ([ref]$i);$startX=$x;$startY=$y;$path.StartFigure();$command='L'}
            'L' {$nx=Read-Number $tokens ([ref]$i);$ny=Read-Number $tokens ([ref]$i);$path.AddLine($x,$y,$nx,$ny);$x=$nx;$y=$ny}
            'V' {$ny=Read-Number $tokens ([ref]$i);$path.AddLine($x,$y,$x,$ny);$y=$ny}
            'C' {$x1=Read-Number $tokens ([ref]$i);$y1=Read-Number $tokens ([ref]$i);$x2=Read-Number $tokens ([ref]$i);$y2=Read-Number $tokens ([ref]$i);$nx=Read-Number $tokens ([ref]$i);$ny=Read-Number $tokens ([ref]$i);$path.AddBezier($x,$y,$x1,$y1,$x2,$y2,$nx,$ny);$x=$nx;$y=$ny}
            'Z' {$path.CloseFigure();$x=$startX;$y=$startY;$command=''}
            default {throw "Unsupported SVG command in icon: $command"}
        }
    }
    return ,$path
}
foreach($variant in @('black','white')) {
    [xml]$svg=Get-Content -LiteralPath (Join-Path $PSScriptRoot "assets\corner-$variant.svg") -Raw
    $images=@()
    foreach($size in @(16,20,24,32,40,48,64,128,256)) {
        $big=New-Object Drawing.Bitmap ($size*4),($size*4),([Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g=[Drawing.Graphics]::FromImage($big)
        $g.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.ScaleTransform([single]($size*4/127),[single]($size*4/127))
        foreach($element in $svg.svg.path) {
            $path=Svg-Path $element.d
            if($element.fill -and $element.fill -ne 'none') {
                $brush=New-Object Drawing.SolidBrush ([Drawing.Color]::FromName([string]$element.fill));$g.FillPath($brush,$path);$brush.Dispose()
            }
            if($element.stroke) {
                $pen=New-Object Drawing.Pen ([Drawing.Color]::FromName([string]$element.stroke)),([single]::Parse($element.'stroke-width',[Globalization.CultureInfo]::InvariantCulture));$g.DrawPath($pen,$path);$pen.Dispose()
            }
            $path.Dispose()
        }
        $g.Dispose();$small=New-Object Drawing.Bitmap $size,$size,([Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g=[Drawing.Graphics]::FromImage($small);$g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.DrawImage($big,0,0,$size,$size);$g.Dispose();$big.Dispose()
        $memory=New-Object IO.MemoryStream;$small.Save($memory,[Drawing.Imaging.ImageFormat]::Png)
        if($size -eq 128){$small.Save((Join-Path $PSScriptRoot "assets\corner-$variant.png"),[Drawing.Imaging.ImageFormat]::Png)}
        $images+=,[pscustomobject]@{Size=$size;Data=$memory.ToArray()};$memory.Dispose();$small.Dispose()
    }
    $stream=[IO.File]::Create((Join-Path $PSScriptRoot "assets\corner-$variant.ico"));$writer=New-Object IO.BinaryWriter $stream
    $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$images.Count)
    $offset=6+16*$images.Count
    foreach($entry in $images) {
        $dimension=if($entry.Size -eq 256){0}else{$entry.Size}
        $writer.Write([byte]$dimension);$writer.Write([byte]$dimension);$writer.Write([byte]0);$writer.Write([byte]0)
        $writer.Write([uint16]1);$writer.Write([uint16]32);$writer.Write([uint32]$entry.Data.Length);$writer.Write([uint32]$offset)
        $offset+=$entry.Data.Length
    }
    foreach($entry in $images){$writer.Write([byte[]]$entry.Data)}
    $writer.Dispose();$stream.Dispose()
}
Get-Item (Join-Path $PSScriptRoot 'assets\corner-*.ico') | Select-Object Name,Length
