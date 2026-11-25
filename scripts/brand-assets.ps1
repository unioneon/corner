$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'docs\images'
New-Item -ItemType Directory -Path $out -Force|Out-Null
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class BrandType {
    [StructLayout(LayoutKind.Sequential)] public struct Rect {public int Left,Top,Right,Bottom;}
    [DllImport("gdi32.dll",CharSet=CharSet.Unicode)] public static extern int AddFontResourceEx(string path,uint flags,IntPtr reserved);
    [DllImport("gdi32.dll",CharSet=CharSet.Unicode)] static extern IntPtr CreateFont(int height,int width,int escape,int orientation,int weight,uint italic,uint underline,uint strike,uint charset,uint output,uint clip,uint quality,uint pitch,string face);
    [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc,IntPtr obj);
    [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr obj);
    [DllImport("gdi32.dll")] static extern int SetBkMode(IntPtr dc,int mode);
    [DllImport("gdi32.dll")] static extern uint SetTextColor(IntPtr dc,uint color);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int DrawText(IntPtr dc,string text,int length,ref Rect rect,uint format);
    public static void Draw(IntPtr dc,string value,int x,int y,int width,int height,int size,bool bold,uint color,string family) {
        var font=CreateFont(-size,0,0,0,bold?700:400,0,0,0,1,0,0,5,0,family);var old=SelectObject(dc,font);SetBkMode(dc,1);SetTextColor(dc,color);
        var rect=new Rect {Left=x,Top=y,Right=x+width,Bottom=y+height};DrawText(dc,value,-1,ref rect,0x20|0x100|0x800);
        SelectObject(dc,old);DeleteObject(font);
    }
}
'@
$family='Segoe UI'
foreach($name in @('Regular','Bold')) {
    $file=Join-Path $root "assets\SF-Pro-Display-$name.otf"
    if(Test-Path -LiteralPath $file){[BrandType]::AddFontResourceEx($file,16,[IntPtr]::Zero)|Out-Null;$family='SF Pro Display'}
}
function Draw-Text($g,[string]$text,[int]$x,[int]$y,[int]$width,[int]$height,[int]$size,[bool]$bold,[Drawing.Color]$color) {
    $dc=$g.GetHdc()
    try{[BrandType]::Draw($dc,$text,($x*$script:brandScale),($y*$script:brandScale),($width*$script:brandScale),($height*$script:brandScale),($size*$script:brandScale),$bold,([uint32]$color.R -bor ([uint32]$color.G -shl 8) -bor ([uint32]$color.B -shl 16)),$family)}finally{$g.ReleaseHdc($dc)}
}
function Round-Path([single]$x,[single]$y,[single]$w,[single]$h,[single]$r) {
    $p=New-Object Drawing.Drawing2D.GraphicsPath;$d=2*$r
    $p.AddArc($x,$y,$d,$d,180,90);$p.AddArc(($x+$w-$d),$y,$d,$d,270,90);$p.AddArc(($x+$w-$d),($y+$h-$d),$d,$d,0,90);$p.AddArc($x,($y+$h-$d),$d,$d,90,90);$p.CloseFigure();return ,$p
}
# The link card uses the same approved poster as the README.
$source=[Drawing.Image]::FromFile((Join-Path $out 'poster.png'))
$image=New-Object Drawing.Bitmap 1280,640,([Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[Drawing.Graphics]::FromImage($image);$g.InterpolationMode='HighQualityBicubic';$g.PixelOffsetMode='HighQuality';$g.DrawImage($source,0,0,1280,640)
$encoder=[Drawing.Imaging.ImageCodecInfo]::GetImageEncoders()|Where-Object {$_.MimeType -eq 'image/jpeg'}
$options=New-Object Drawing.Imaging.EncoderParameters 1
$options.Param[0]=New-Object Drawing.Imaging.EncoderParameter ([Drawing.Imaging.Encoder]::Quality),([long]95)
$image.Save((Join-Path $out 'social-preview.jpg'),$encoder,$options)
$options.Dispose();$g.Dispose();$image.Dispose();$source.Dispose();$script:brandScale=1
if((Get-Item (Join-Path $out 'social-preview.jpg')).Length -ge 1000000){throw 'Social preview must be below 1 MB.'}
# Deterministic installer artwork uses the same existing vector identity.
$image=New-Object Drawing.Bitmap 164,314,([Drawing.Imaging.PixelFormat]::Format24bppRgb);$g=[Drawing.Graphics]::FromImage($image);$g.Clear([Drawing.Color]::FromArgb(25,110,254));$g.InterpolationMode='HighQualityBicubic'
$logo=[Drawing.Image]::FromFile((Join-Path $root 'assets\corner-white.png'));$g.DrawImage($logo,42,56,80,80)
Draw-Text $g 'corner' 25 151 140 42 31 $true ([Drawing.Color]::White)
Draw-Text $g 'A softer edge' 24 222 140 28 17 $false ([Drawing.Color]::White)
Draw-Text $g 'to Windows.' 24 248 140 28 17 $false ([Drawing.Color]::White)
$image.Save((Join-Path $out 'installer-side.bmp'),[Drawing.Imaging.ImageFormat]::Bmp);$g.Dispose();$image.Dispose();$logo.Dispose()
$image=New-Object Drawing.Bitmap 55,55,([Drawing.Imaging.PixelFormat]::Format24bppRgb);$g=[Drawing.Graphics]::FromImage($image);$g.Clear([Drawing.Color]::White);$g.InterpolationMode='HighQualityBicubic';$logo=[Drawing.Image]::FromFile((Join-Path $root 'assets\corner-black.png'));$g.DrawImage($logo,5,5,45,45);$image.Save((Join-Path $out 'installer-small.bmp'),[Drawing.Imaging.ImageFormat]::Bmp);$g.Dispose();$image.Dispose();$logo.Dispose()
Get-Item (Join-Path $out 'social-preview.jpg')|Select-Object Name,Length
