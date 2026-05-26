<#
.SYNOPSIS
  Assemble the standalone Windows install tree for Hamster from a Release build,
  then (optionally) produce the Inno Setup installer.

.DESCRIPTION
  Stages dist\staging\ with the exact runtime layout the editor expects:

      dist\staging\
        Hamster-Wheel\
          Hamster-Wheel.exe
          python311.dll  python311.zip  python311._pth   (CPython embeddable, isolated)
          vcruntime140.dll  vcruntime140_1.dll  msvcp140.dll   (app-local MSVC runtime)
          ...rest of the embeddable stdlib extension DLLs/pyds...
        share\Resources\Hamster-Wheel\Resources\
          Fonts\ Icons\ Logo\ Sprites\ Packages\Hamster.cp311-win_amd64.pyd

  The exe resolves resources via <exe>\..\share\Resources\..., so the two-level
  split above is load-bearing. A file-manifest assertion fails the build before
  any installer is produced if a required piece is missing (spec Risks #2/#3/#4).

.PARAMETER BuildDir
  Release build directory. Default: <repo>\build-release.

.PARAMETER MakeInstaller
  After staging, run Inno Setup (iscc) on Hamster.iss to emit dist\HamsterSetup.exe.

.PARAMETER Run
  After staging, launch the staged exe -- smoke-tests that the *bundled* runtime
  loads (no dev Python on PATH is used because python311.dll sits next to the exe).

.NOTES
  Prereqs: a Release build (cmake --build build-release ...); internet on first run
  (fetches the pinned Python embeddable into Scripts\.cache); Inno Setup 6 only when
  -MakeInstaller is passed.

  ASCII-only by design: Windows PowerShell 5.1 reads BOM-less .ps1 as ANSI, so
  non-ASCII punctuation here would mis-decode and break parsing.
#>
[CmdletBinding()]
param(
  [string]$BuildDir,
  [switch]$MakeInstaller,
  [switch]$Run
)

$ErrorActionPreference = "Stop"

# --- Paths -----------------------------------------------------------------
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BuildDir) { $BuildDir = Join-Path $RepoRoot "build-release" }
$Cache    = Join-Path $PSScriptRoot ".cache"
$DistDir  = Join-Path $RepoRoot "dist"
$Staging  = Join-Path $DistDir "staging"
$AppDir   = Join-Path $Staging "Hamster-Wheel"           # exe + bundled runtime
$ShareSrc = Join-Path $BuildDir "share"                  # built resource tree
$ShareDst = Join-Path $Staging "share"

# --- Pinned Python embeddable (must match the .pyd's cp311 ABI) -----------
$PyVersion  = "3.11.9"
$PyEmbedZip = "python-$PyVersion-embed-amd64.zip"
$PyEmbedUrl = "https://www.python.org/ftp/python/$PyVersion/$PyEmbedZip"
$PyEmbedSha = "009D6BF7E3B2DDCA3D784FA09F90FE54336D5B60F0E0F305C37F400BF83CFD3B"

function Fail($msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }

# --- 1. Verify build artifacts --------------------------------------------
Step "Verifying Release build at $BuildDir"
$ExeSrc = Join-Path $BuildDir "Hamster-Wheel\Hamster-Wheel.exe"
$PydSrc = Join-Path $BuildDir "Hamster-Py\Hamster.cp311-win_amd64.pyd"
if (-not (Test-Path $ExeSrc))   { Fail "missing $ExeSrc - run the Release build first" }
if (-not (Test-Path $PydSrc))   { Fail "missing $PydSrc - build the Hamster target (Release)" }
if (-not (Test-Path $ShareSrc)) { Fail "missing $ShareSrc - resource tree not built" }

# --- 2. Clean staging ------------------------------------------------------
Step "Cleaning staging dir"
Remove-Item -Recurse -Force $Staging -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $AppDir | Out-Null

# --- 3. Copy exe + resource tree ------------------------------------------
Step "Copying exe and share\Resources tree"
Copy-Item $ExeSrc -Destination $AppDir
Copy-Item $ShareSrc -Destination $Staging -Recurse
# Force the freshly-built .pyd in (guards against a stale copy in the share tree).
$PkgDir = Join-Path $ShareDst "Resources\Hamster-Wheel\Resources\Packages"
New-Item -ItemType Directory -Force -Path $PkgDir | Out-Null
Copy-Item $PydSrc -Destination $PkgDir -Force

# Shaders are runtime resources loaded by path, but CMake doesn't copy them into
# share/ -- the dev build reads them straight from the source tree. Stage them
# here so the installed exe finds them under <exe>/../share/Resources/Hamster-Core/.
$ShaderSrc = Join-Path $RepoRoot "Hamster-Core\src\Renderer\DefaultShaders"
$ShaderDst = Join-Path $ShareDst "Resources\Hamster-Core\Renderer\DefaultShaders"
New-Item -ItemType Directory -Force -Path $ShaderDst | Out-Null
Copy-Item (Join-Path $ShaderSrc "*") -Destination $ShaderDst -Recurse -Force

# --- 4. Fetch + verify the Python embeddable ------------------------------
Step "Fetching Python $PyVersion embeddable (cached)"
New-Item -ItemType Directory -Force -Path $Cache | Out-Null
$ZipPath = Join-Path $Cache $PyEmbedZip
$needDownload = $true
if (Test-Path $ZipPath) {
  $h = (Get-FileHash -Algorithm SHA256 $ZipPath).Hash
  if ($h -eq $PyEmbedSha) { $needDownload = $false; Write-Host "    cache hit (sha256 ok)" }
  else { Write-Host "    cached zip hash mismatch - re-downloading" }
}
if ($needDownload) {
  Invoke-WebRequest -Uri $PyEmbedUrl -OutFile $ZipPath -UseBasicParsing
  $h = (Get-FileHash -Algorithm SHA256 $ZipPath).Hash
  if ($h -ne $PyEmbedSha) { Fail "embeddable sha256 mismatch: got $h expected $PyEmbedSha" }
}

# --- 5. Extract embeddable next to the exe (drop the standalone launchers) -
Step "Extracting embeddable into app dir"
Expand-Archive -Path $ZipPath -DestinationPath $AppDir -Force
Remove-Item (Join-Path $AppDir "python.exe")  -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $AppDir "pythonw.exe") -Force -ErrorAction SilentlyContinue

# Isolated, self-locating sys.path: stdlib zip + the app dir; site disabled.
# Runtime sys.path.append (AddPathToPy) still adds the project dir on top.
@"
python311.zip
.

# isolated embeddable runtime - do not enable site
#import site
"@ | Set-Content -Path (Join-Path $AppDir "python311._pth") -Encoding ascii

# --- 6. App-local MSVC runtime --------------------------------------------
# The embeddable ships its own (older) vcruntime140*.dll for python311.dll, but
# our exe is built with a newer MSVC STL -- overwrite with the newer redist DLLs
# (forward-compatible for python) and add msvcp140.dll (C++ STL, not in embed).
Step "Staging app-local MSVC runtime DLLs"
$vcNames = "vcruntime140.dll","vcruntime140_1.dll","msvcp140.dll"
# Pick the redist with the highest VERSION across every VS install -- not the
# highest edition name. The shipped runtime must be >= the toolset that built
# the exe (14.51), and a lexical edition sort wrongly ranks "2022" above "18".
$candidates = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Directory -ErrorAction SilentlyContinue |
  ForEach-Object { Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue } |
  ForEach-Object { Get-ChildItem (Join-Path $_.FullName "VC\Redist\MSVC") -Directory -ErrorAction SilentlyContinue } |
  ForEach-Object {
    $crt = Get-ChildItem (Join-Path $_.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($crt -and (Test-Path (Join-Path $crt.FullName "msvcp140.dll")) -and
        (Test-Path (Join-Path $crt.FullName "vcruntime140_1.dll"))) {
      $v = $null; [void][version]::TryParse($_.Name, [ref]$v)
      [pscustomobject]@{ Version = $v; Path = $crt.FullName }
    }
  } | Where-Object { $_.Version } | Sort-Object Version -Descending
$redistRoot = if ($candidates) { $candidates[0].Path } else { $null }
foreach ($n in $vcNames) {
  $src = if ($redistRoot) { Join-Path $redistRoot $n } else { Join-Path $env:WINDIR "System32\$n" }
  if (-not (Test-Path $src)) { Fail "MSVC runtime DLL not found: $n" }
  Copy-Item $src -Destination $AppDir -Force
}
Write-Host "    sourced from: $(if ($redistRoot) { $redistRoot } else { 'System32 (fallback)' })"

# --- 7. Manifest assertion -------------------------------------------------
Step "Asserting install manifest"
$required = @(
  "Hamster-Wheel\Hamster-Wheel.exe",
  "Hamster-Wheel\python311.dll",
  "Hamster-Wheel\python311.zip",
  "Hamster-Wheel\python311._pth",
  "Hamster-Wheel\vcruntime140.dll",
  "Hamster-Wheel\vcruntime140_1.dll",
  "Hamster-Wheel\msvcp140.dll",
  "share\Resources\Hamster-Wheel\Resources\Packages\Hamster.cp311-win_amd64.pyd"
)
$requiredDirs = @(
  "share\Resources\Hamster-Wheel\Resources\Fonts",
  "share\Resources\Hamster-Wheel\Resources\Icons",
  "share\Resources\Hamster-Wheel\Resources\Logo"
)
# The 5 shader pairs the renderer loads at startup (missing -> blank/garbage UI).
$shaderRoot = "share\Resources\Hamster-Core\Renderer\DefaultShaders"
foreach ($s in @("Sprite","Flat","SpriteBatch","UIRect","UIText")) {
  $required += "$shaderRoot\${s}Shader.vs"
  $required += "$shaderRoot\${s}Shader.fs"
}
$missing = @()
foreach ($r in $required) {
  if (-not (Test-Path (Join-Path $Staging $r))) { $missing += $r }
}
foreach ($d in $requiredDirs) {
  $p = Join-Path $Staging $d
  if (-not (Test-Path $p) -or -not (Get-ChildItem $p -ErrorAction SilentlyContinue)) { $missing += "$d\ (non-empty)" }
}
if ($missing.Count -gt 0) { Fail "manifest incomplete:`n  $($missing -join "`n  ")" }
Write-Host "    manifest OK ($($required.Count) files + $($requiredDirs.Count) dirs)"

Write-Host "Staging complete: $Staging" -ForegroundColor Green

# --- 8. Optional: smoke the staged runtime --------------------------------
if ($Run) {
  Step "Launching staged exe"
  Start-Process -FilePath (Join-Path $AppDir "Hamster-Wheel.exe")
}

# --- 9. Optional: build the installer -------------------------------------
if ($MakeInstaller) {
  Step "Building installer with Inno Setup"
  $iscc = (Get-Command iscc.exe -ErrorAction SilentlyContinue).Source
  if (-not $iscc) {
    foreach ($c in @("C:\Program Files (x86)\Inno Setup 6\ISCC.exe","C:\Program Files\Inno Setup 6\ISCC.exe")) {
      if (Test-Path $c) { $iscc = $c; break }
    }
  }
  if (-not $iscc) { Fail "iscc.exe not found - install Inno Setup 6 (https://jrsoftware.org/isdl.php)" }
  $iss = Join-Path $RepoRoot "Hamster.iss"
  if (-not (Test-Path $iss)) { Fail "missing $iss" }
  & $iscc $iss
  if ($LASTEXITCODE -ne 0) { Fail "iscc failed ($LASTEXITCODE)" }
  Write-Host "Installer built: $(Join-Path $DistDir 'HamsterSetup.exe')" -ForegroundColor Green
}
