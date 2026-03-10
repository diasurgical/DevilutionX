<#
.SYNOPSIS
  Builds a Windows Release package and writes it to build\releases.

.DESCRIPTION
  - Runs CMake configure (if needed) and builds the project in Release mode.
  - Copies the built exe (and required runtime files) directly into build\releases
    (no versioned subfolder).
  - Also writes a versioned zip (diabloaccess-<tag>-windows-x64.zip) into build\releases.

.EXAMPLE
  .\build_release.ps1

.EXAMPLE
  .\build_release.ps1 -BuildDir build -Config Release
#>

[CmdletBinding()]
param(
	[ValidateSet('Release', 'RelWithDebInfo', 'Debug')]
	[string]$Config = 'Release',

	[string]$BuildDir = 'build',

	# Where to write the packaged release (folder + zip).
	# Default matches the typical CMake multi-config output dir on Windows.
	[string]$OutputDir = 'build\\releases',

	# Layout:
	# - Flat: write files directly into $OutputDir (exe is in $OutputDir\devilutionx.exe).
	# - Versioned: create $OutputDir\<packageName>\... and zip it.
	[ValidateSet('Flat', 'Versioned')]
	[string]$Layout = 'Flat'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-Version {
	param([string]$RepoRoot)

	$version = $null
	if (Get-Command git -ErrorAction SilentlyContinue) {
		try {
			$version = (git -C $RepoRoot describe --tags --abbrev=0 2>$null).Trim()
		} catch {
			$version = $null
		}
	}
	if ([string]::IsNullOrWhiteSpace($version)) {
		# Fallback: timestamp-based version
		$version = (Get-Date -Format 'yyyyMMdd-HHmm')
	}
	return $version
}

$repoRoot = (Resolve-Path $PSScriptRoot).Path
$buildDirPath = Join-Path $repoRoot $BuildDir
$outputDirPath = Join-Path $repoRoot $OutputDir

$cmakeCache = Join-Path $buildDirPath 'CMakeCache.txt'
if (-not (Test-Path $cmakeCache)) {
	Write-Host "Configuring CMake: $buildDirPath"
	& cmake -S $repoRoot -B $buildDirPath | Out-Host
}

Write-Host "Building: config=$Config"
& cmake --build $buildDirPath --config $Config | Out-Host

$buildOutputDir = Join-Path $buildDirPath $Config
if (-not (Test-Path $buildOutputDir)) {
	throw "Expected build output directory not found: $buildOutputDir"
}

$version = Resolve-Version -RepoRoot $repoRoot
$packageName = "diabloaccess-$version-windows-x64"
$zipPath = Join-Path $outputDirPath "$packageName.zip"

# Stage into a temp dir first, then (optionally) copy to output.
$stagingDir = if ($Layout -eq 'Versioned') {
	Join-Path $outputDirPath $packageName
} else {
	Join-Path $outputDirPath '_staging'
}

New-Item -ItemType Directory -Force $outputDirPath | Out-Null
if (Test-Path $stagingDir) {
	Remove-Item -Recurse -Force $stagingDir
}

New-Item -ItemType Directory -Force $stagingDir | Out-Null
foreach ($d in @('assets', 'audio', 'mods')) {
	New-Item -ItemType Directory -Force (Join-Path $stagingDir $d) | Out-Null
}

$runtimeFiles = @(
	'devilutionx.exe',
	'SDL2.dll',
	'Tolk.dll',
	'zlib.dll',
	'nvdaControllerClient64.dll',
	'SAAPI64.dll',
	'ZDSRAPI.ini',
	'ZDSRAPI_x64.dll'
)

foreach ($file in $runtimeFiles) {
	$src = Join-Path $buildOutputDir $file
	if (-not (Test-Path $src)) {
		throw "Missing runtime file in build output: $src"
	}
	Copy-Item $src $stagingDir -Force
}

$readmeSrc = Join-Path $repoRoot 'README.md'
if (Test-Path $readmeSrc) {
	Copy-Item $readmeSrc (Join-Path $stagingDir 'README.md') -Force
} else {
	Write-Warning "README.md not found at repo root; skipping."
}

# Assets: prefer build\assets (generated + trimmed) and fall back to build\<Config>\assets if needed.
$assetsSrc = Join-Path $buildDirPath 'assets'
if (-not (Test-Path $assetsSrc)) {
	$assetsSrc = Join-Path $buildOutputDir 'assets'
}
if (-not (Test-Path $assetsSrc)) {
	throw "Assets directory not found (expected build\\assets or build\\$Config\\assets)."
}
Copy-Item (Join-Path $assetsSrc '*') (Join-Path $stagingDir 'assets') -Recurse -Force

# Audio + mods are copied from build\<Config>\...
$audioSrc = Join-Path $buildOutputDir 'audio'
$modsSrc = Join-Path $buildOutputDir 'mods'
if (-not (Test-Path $audioSrc)) { throw "Audio directory not found: $audioSrc" }
if (-not (Test-Path $modsSrc)) { throw "Mods directory not found: $modsSrc" }
Copy-Item (Join-Path $audioSrc '*') (Join-Path $stagingDir 'audio') -Recurse -Force
Copy-Item (Join-Path $modsSrc '*') (Join-Path $stagingDir 'mods') -Recurse -Force

if (Test-Path $zipPath) {
	Remove-Item -Force $zipPath
}
$tmpZip = Join-Path ([System.IO.Path]::GetTempPath()) "$packageName-$([System.Guid]::NewGuid().ToString('N')).zip"
Compress-Archive -Path (Join-Path $stagingDir '*') -DestinationPath $tmpZip -Force
Move-Item -Force $tmpZip $zipPath

if ($Layout -eq 'Flat') {
	# Clean old release files (keep anything else the user may have in the folder).
	$knownDirs = @('assets', 'audio', 'mods')
	foreach ($d in $knownDirs) {
		$dst = Join-Path $outputDirPath $d
		if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
	}
	foreach ($file in $runtimeFiles + @('README.md')) {
		$dst = Join-Path $outputDirPath $file
		if (Test-Path $dst) { Remove-Item -Force $dst }
	}

	# Copy staged content directly into output dir (exe ends up in build\releases\devilutionx.exe).
	Copy-Item (Join-Path $stagingDir '*') $outputDirPath -Recurse -Force
	Remove-Item -Recurse -Force $stagingDir
	Write-Host "Release folder (flat): $outputDirPath"
} else {
	Write-Host "Release folder: $stagingDir"
}
Write-Host "Release zip:           $zipPath"
