# Draws the YouTube thumbnail (1280 x 720) for Teensy Chess with System.Drawing.
# Uses the UI preview screenshot tools/ui_preview/out/4_mate_flipped.png.
# Run: powershell -NoProfile -File youtube/make_thumbnail.ps1

Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$shotPath = Join-Path $here '..\tools\ui_preview\out\4_mate_flipped.png'
$outPath = Join-Path $here 'thumbnail.png'

$W = 1280; $H = 720
$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.TextRenderingHint = 'AntiAliasGridFit'
$g.CompositingQuality = 'HighQuality'

function C($a, $r, $gr, $b) { [System.Drawing.Color]::FromArgb($a, $r, $gr, $b) }
function RoundRect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = 2 * $r
  $p.AddArc($x, $y, $d, $d, 180, 90)
  $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
  $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
  $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
  $p.CloseFigure()
  $p
}
function RadialBrush([float]$cx, [float]$cy, [float]$rx, [float]$ry, $inner, $outer) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $p.AddEllipse($cx - $rx, $cy - $ry, 2 * $rx, 2 * $ry)
  $b = New-Object System.Drawing.Drawing2D.PathGradientBrush $p
  $b.CenterPoint = New-Object System.Drawing.PointF $cx, $cy
  $b.CenterColor = $inner
  $b.SurroundColors = [System.Drawing.Color[]]@($outer)
  @{ Brush = $b; Path = $p }
}
function VGradient([float]$y0, [float]$y1, $colors, [float[]]$stops) {
  $b = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.PointF 0, $y0), (New-Object System.Drawing.PointF 0, $y1), $colors[0], $colors[-1]
  $blend = New-Object System.Drawing.Drawing2D.ColorBlend
  $blend.Colors = [System.Drawing.Color[]]$colors
  $blend.Positions = $stops
  $b.InterpolationColors = $blend
  $b.WrapMode = 'TileFlipX'
  $b
}
$impact = New-Object System.Drawing.FontFamily 'Impact'

# Headline text: black drop shadow, colored glow, thick black outline, gradient fill.
# Right-aligned at xRight; y is the top of the text's bounding box.
function BigText([string]$text, [float]$size, [float]$xRight, [float]$y, $colors, $glow) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $p.AddString($text, $impact, 0, $size, (New-Object System.Drawing.PointF 0, 0), [System.Drawing.StringFormat]::GenericTypographic)
  $bb = $p.GetBounds()
  $m = New-Object System.Drawing.Drawing2D.Matrix
  $m.Translate($xRight - $bb.Right, $y - $bb.Top)
  $p.Transform($m)
  $bb = $p.GetBounds()
  foreach ($wd in 34, 24, 16) {  # glow
    $pen = New-Object System.Drawing.Pen (C 40 $glow.R $glow.G $glow.B), $wd
    $pen.LineJoin = 'Round'
    $g.DrawPath($pen, $p)
  }
  $shadow = $p.Clone(); $sm = New-Object System.Drawing.Drawing2D.Matrix; $sm.Translate(7, 9); $shadow.Transform($sm)
  $spen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), 10; $spen.LineJoin = 'Round'
  $g.DrawPath($spen, $shadow); $g.FillPath([System.Drawing.Brushes]::Black, $shadow)
  $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), 10; $pen.LineJoin = 'Round'
  $g.DrawPath($pen, $p)
  $g.FillPath((VGradient $bb.Top $bb.Bottom $colors ([float[]]@(0, 0.45, 1))), $p)
  $bb
}

# --- Background -----------------------------------------------------------------
$bg = RadialBrush 400 380 1100 800 (C 255 0x26 0x33 0x72) (C 255 3 5 16)
$g.FillRectangle([System.Drawing.Brushes]::Black, 0, 0, $W, $H)
$g.FillPath($bg.Brush, $bg.Path)

# Sunburst rays centered behind the display
$cx = 378; $cy = 414
$rays = New-Object System.Drawing.Drawing2D.GraphicsPath
for ($a = 0; $a -lt 360; $a += 15) { $rays.AddPie($cx - 1000, $cy - 1000, 2000, 2000, $a, 6.5) }
$rb = RadialBrush $cx $cy 900 900 (C 120 255 180 0) (C 0 255 120 0)
$g.SetClip($rb.Path); $g.FillPath($rb.Brush, $rays); $g.ResetClip()

$glow = RadialBrush $cx $cy 420 330 (C 170 255 140 0) (C 0 255 60 0)
$g.FillPath($glow.Brush, $glow.Path)

# --- TFT module with the screenshot -------------------------------------------
$shot = [System.Drawing.Image]::FromFile((Resolve-Path $shotPath))
$state = $g.Save()
$g.TranslateTransform($cx, $cy); $g.RotateTransform(-4); $g.TranslateTransform(-348, -270)
# shadow and orange halo
$g.FillPath((New-Object System.Drawing.SolidBrush (C 150 0 0 0)), (RoundRect 14 30 696 540 16))
foreach ($wd in 40, 26, 14) {
  $g.DrawPath((New-Object System.Drawing.Pen (C 45 255 130 0), $wd), (RoundRect 0 0 696 540 14))
}
$red = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.PointF 0, 0), (New-Object System.Drawing.PointF 696, 540), (C 255 0xE8 0x2A 0x30), (C 255 0x9E 0x12 0x18)
$g.FillPath($red, (RoundRect 0 0 696 540 14))
$g.DrawPath((New-Object System.Drawing.Pen (C 255 0x5C 0x0A 0x0D), 4), (RoundRect 0 0 696 540 14))
foreach ($pt in @(@(8, 8), @(670, 8), @(8, 514), @(670, 514))) {
  $g.FillEllipse((New-Object System.Drawing.SolidBrush (C 255 0x7A 0x53 0x10)), $pt[0], $pt[1], 18, 18)
  $g.FillEllipse((New-Object System.Drawing.SolidBrush (C 255 0xF2 0xC7 0x5C)), $pt[0] + 4, $pt[1] + 4, 10, 10)
}
$g.FillPath([System.Drawing.Brushes]::Black, (RoundRect 20 22 656 496 6))
$g.InterpolationMode = 'NearestNeighbor'; $g.PixelOffsetMode = 'Half'
$g.DrawImage($shot, 28, 30, 640, 480)
$g.PixelOffsetMode = 'Default'; $g.InterpolationMode = 'HighQualityBicubic'
$shine = New-Object System.Drawing.Drawing2D.GraphicsPath
$shine.AddPolygon([System.Drawing.PointF[]]@((New-Object System.Drawing.PointF 28, 30), (New-Object System.Drawing.PointF 330, 30), (New-Object System.Drawing.PointF 28, 300)))
$g.FillPath((New-Object System.Drawing.SolidBrush (C 34 255 255 255)), $shine)
$g.Restore($state)

# --- CHECKMATE starburst sticker ------------------------------------------------
$state = $g.Save()
$g.TranslateTransform(150, 150); $g.RotateTransform(-14); $g.ScaleTransform(0.85, 0.85)
$star = New-Object System.Drawing.Drawing2D.GraphicsPath
$pts = for ($i = 0; $i -lt 32; $i++) {
  $ang = [Math]::PI * 2 * $i / 32; $r = if ($i % 2) { 112 } else { 150 }
  New-Object System.Drawing.PointF ([float]($r * [Math]::Cos($ang))), ([float]($r * [Math]::Sin($ang)))
}
$star.AddPolygon([System.Drawing.PointF[]]$pts)
foreach ($wd in 36, 22) { $g.DrawPath((New-Object System.Drawing.Pen (C 60 255 220 0), $wd), $star) }
$st = $star.Clone(); $mm = New-Object System.Drawing.Drawing2D.Matrix; $mm.Translate(0, 10); $st.Transform($mm)
$g.FillPath([System.Drawing.Brushes]::Black, $st)
$sb = New-Object System.Drawing.Drawing2D.PathGradientBrush $star
$sb.CenterColor = C 255 255 245 80; $sb.SurroundColors = [System.Drawing.Color[]]@((C 255 255 150 0))
$g.FillPath($sb, $star)
$g.DrawPath((New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), 5), $star)
$fmt = New-Object System.Drawing.StringFormat; $fmt.Alignment = 'Center'
foreach ($line in @(@('CHECK', 58, -78), @('MATE!', 58, -18))) {
  $tp = New-Object System.Drawing.Drawing2D.GraphicsPath
  $tp.AddString($line[0], $impact, 0, $line[1], (New-Object System.Drawing.PointF 0, $line[2]), $fmt)
  $pen = New-Object System.Drawing.Pen (C 255 0x4A 0 8), 6; $pen.LineJoin = 'Round'
  $g.DrawPath($pen, $tp); $g.FillPath((New-Object System.Drawing.SolidBrush (C 255 0xE0 0 0x1C)), $tp)
}
$tp = New-Object System.Drawing.Drawing2D.GraphicsPath
$tp.AddString('IT BEAT ME!', $impact, 0, 28, (New-Object System.Drawing.PointF 0, 46), $fmt)
$g.FillPath((New-Object System.Drawing.SolidBrush (C 255 20 20 20)), $tp)
$g.Restore($state)

# --- Headline ---------------------------------------------------------------------
$yellow = @((C 255 255 250 180), (C 255 255 212 0), (C 255 255 130 0))
$mint = @((C 255 210 255 245), (C 255 45 255 196), (C 255 0 170 125))
$right = 1245
$b1 = BigText 'STOCKFISH' 138 $right 30 $yellow ([System.Drawing.Color]::Orange)
$b2 = BigText 'RUNNING ON A' 58 $right ($b1.Bottom + 22) @((C 255 255 255 255), (C 255 255 255 255), (C 255 215 225 255)) ([System.Drawing.Color]::White)
$b3 = BigText 'TEENSY 4.0' 124 $right ($b2.Bottom + 22) $mint ([System.Drawing.Color]::Aquamarine)

# --- Note about the Teensy and Stockfish --------------------------------------------
$noteX = 745; $noteY = $b3.Bottom + 34; $noteW = $right - $noteX; $noteH = 128
$g.FillPath((New-Object System.Drawing.SolidBrush (C 190 0 0 0)), (RoundRect $noteX $noteY $noteW $noteH 14))
foreach ($wd in 16, 9) { $g.DrawPath((New-Object System.Drawing.Pen (C 50 45 255 196), $wd), (RoundRect $noteX $noteY $noteW $noteH 14)) }
$g.DrawPath((New-Object System.Drawing.Pen (C 255 45 255 196), 3), (RoundRect $noteX $noteY $noteW $noteH 14))
$font = New-Object System.Drawing.Font 'Segoe UI', 23, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$typo = [System.Drawing.StringFormat]::GenericTypographic.Clone()
$typo.FormatFlags = $typo.FormatFlags -bor [System.Drawing.StringFormatFlags]::MeasureTrailingSpaces
$white = [System.Drawing.Brushes]::White; $gold = New-Object System.Drawing.SolidBrush (C 255 255 212 0)
# Text segments; "`n" at the end of a segment starts a new line
$segs = @(
  @{ T = "The legendary chess engine, ported`n"; B = $white },
  @{ T = 'to a '; B = $white }, @{ T = '$24 microcontroller'; B = $gold }, @{ T = ": 600 MHz,`n"; B = $white },
  @{ T = '1 MB of RAM. '; B = $white }, @{ T = 'No PC. No internet!'; B = $gold }
)
$tx = $noteX + 20; $ty = $noteY + 14
foreach ($seg in $segs) {
  $t = $seg.T.TrimEnd("`n")
  $g.DrawString($t, $font, $seg.B, $tx, $ty, $typo)
  $tx += $g.MeasureString($t, $font, 2000, $typo).Width
  if ($seg.T.EndsWith("`n")) { $tx = $noteX + 20; $ty += 33 }
}

# --- Teensy 4.0 board ----------------------------------------------------------------
$state = $g.Save()
$g.TranslateTransform(1050, 652); $g.RotateTransform(-3); $g.TranslateTransform(-170, -46)
foreach ($wd in 30, 18) { $g.DrawPath((New-Object System.Drawing.Pen (C 50 45 255 196), $wd), (RoundRect 0 0 340 92 8)) }
$g.FillPath((New-Object System.Drawing.SolidBrush (C 160 0 0 0)), (RoundRect 8 14 340 92 8))
$green = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.PointF 0, 0), (New-Object System.Drawing.PointF 0, 92), (C 255 0x22 0x9A 0x40), (C 255 0x0E 0x58 0x20)
$g.FillPath($green, (RoundRect 0 0 340 92 8))
$g.DrawPath((New-Object System.Drawing.Pen (C 255 6 0x3D 0x14), 3), (RoundRect 0 0 340 92 8))
$usb = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.PointF 0, 26), (New-Object System.Drawing.PointF 0, 66), (C 255 245 245 245), (C 255 150 150 150)
$g.FillPath($usb, (RoundRect -24 26 40 40 4))
for ($i = 0; $i -lt 14; $i++) {
  foreach ($py in 5, 77) {
    $px = 24 + $i * 22
    $g.FillEllipse((New-Object System.Drawing.SolidBrush (C 255 0x8A 0x65 0x14)), $px, $py, 10, 10)
    $g.FillEllipse((New-Object System.Drawing.SolidBrush (C 255 0xE8 0xC2 0x5A)), $px + 3, $py + 3, 4, 4)
  }
}
$g.FillRectangle((New-Object System.Drawing.SolidBrush (C 255 58 58 58)), 117, 19, 62, 54)
$g.FillRectangle((New-Object System.Drawing.SolidBrush (C 255 21 21 21)), 120, 22, 56, 48)
$small = New-Object System.Drawing.Font 'Arial', 10, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$g.DrawString("i.MX`nRT1062", $small, (New-Object System.Drawing.SolidBrush (C 255 160 160 160)), 148, 33, $fmt)
$lbl = New-Object System.Drawing.Font 'Arial Black', 22, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$g.DrawString('TEENSY', $lbl, $white, 196, 22)
$lbl2 = New-Object System.Drawing.Font 'Arial', 14, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$g.DrawString('4.0 - 600 MHz', $lbl2, (New-Object System.Drawing.SolidBrush (C 255 200 255 215)), 200, 52)
$g.Restore($state)

# --- Arrow pointing at the mated king -------------------------------------------
$state = $g.Save()
$g.TranslateTransform(388, 706); $g.RotateTransform(216)
$arrow = New-Object System.Drawing.Drawing2D.GraphicsPath
$arrow.AddPolygon([System.Drawing.PointF[]]@(
  (New-Object System.Drawing.PointF 0, -12), (New-Object System.Drawing.PointF 60, -12), (New-Object System.Drawing.PointF 60, -34),
  (New-Object System.Drawing.PointF 100, 0), (New-Object System.Drawing.PointF 60, 34), (New-Object System.Drawing.PointF 60, 12),
  (New-Object System.Drawing.PointF 0, 12)))
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), 8; $pen.LineJoin = 'Round'
$g.DrawPath($pen, $arrow); $g.FillPath((New-Object System.Drawing.SolidBrush (C 255 255 212 0)), $arrow)
$g.Restore($state)

$bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose(); $shot.Dispose()
"saved $outPath"
