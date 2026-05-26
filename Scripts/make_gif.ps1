<#
.SYNOPSIS
  Convert a screen recording (mp4/mov/mkv) into a README-ready optimized GIF.

.DESCRIPTION
  Two-pass ffmpeg: generate a per-clip palette, then map with dithering. This
  is what keeps a screen-capture GIF small AND clean (a single global palette
  is the usual cause of banding + bloat). Tuned defaults for GitHub READMEs:
  720px wide, 14 fps. Aim for < 5 MB per GIF.

  Requires ffmpeg on PATH:  winget install Gyan.FFmpeg   (then reopen shell)

.EXAMPLE
  ./Scripts/make_gif.ps1 -In recording.mp4 -Out docs/demo-play.gif
.EXAMPLE
  # trim to a 9s window starting at 3s, narrower + slower for a small file
  ./Scripts/make_gif.ps1 -In raw.mp4 -Out docs/demo-editor.gif -Start 3 -Duration 9 -Width 640 -Fps 12
#>
param(
  [Parameter(Mandatory = $true)][string]$In,
  [Parameter(Mandatory = $true)][string]$Out,
  [int]$Width = 720,
  [int]$Fps = 14,
  [double]$Start = 0,        # seconds to skip from the start (INPUT time)
  [double]$Duration = 0,     # 0 = whole clip (INPUT time, before speed-up)
  [double]$Speed = 1.0       # >1 speeds up (e.g. 2 = half the length); good
                             # for editor/setup montages, leave 1 for gameplay
)

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
  Write-Error "ffmpeg not found. Install: winget install Gyan.FFmpeg  (then reopen the shell)"
  exit 1
}
if (-not (Test-Path $In)) { Write-Error "Input not found: $In"; exit 1 }

$trim = ""
if ($Start -gt 0)    { $trim += " -ss $Start" }
if ($Duration -gt 0) { $trim += " -t $Duration" }

$palette = [System.IO.Path]::GetTempFileName() + ".png"
# setpts speeds the clip up (PTS/Speed); then resample to target fps + width.
$speedFilter = if ($Speed -ne 1.0) { "setpts=PTS/$Speed," } else { "" }
$fc = "$speedFilter" + "fps=$Fps,scale=$Width`:-1:flags=lanczos"

# Pass 1: palette from the (trimmed) clip.
Invoke-Expression "ffmpeg -y$trim -i `"$In`" -vf `"$fc,palettegen=stats_mode=diff`" `"$palette`"" | Out-Null
# Pass 2: map using the palette with Bayer dithering (small, stable).
Invoke-Expression "ffmpeg -y$trim -i `"$In`" -i `"$palette`" -lavfi `"$fc [x]; [x][1:v] paletteuse=dither=bayer:bayer_scale=3`" `"$Out`"" | Out-Null
Remove-Item $palette -ErrorAction SilentlyContinue

if (Test-Path $Out) {
  $mb = [math]::Round((Get-Item $Out).Length / 1MB, 2)
  Write-Host "Wrote $Out  ($mb MB, ${Width}px, ${Fps}fps)" -ForegroundColor Green
  if ($mb -gt 5) { Write-Warning "Over 5 MB - try -Width 600 / -Fps 12 / shorter -Duration." }
} else {
  Write-Error "ffmpeg produced no output - check the messages above."
}
