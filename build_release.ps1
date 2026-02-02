<#
.SYNOPSIS
  Builds a Windows Release package and writes it to build\Release.

.DESCRIPTION
  - Runs CMake configure (if needed) and builds the project in Release mode.
  - Creates a portable "release" folder and a zip with the runtime files
    (exe + required DLLs + assets/audio/mods + README.md).
  - Output location (by default): build\Release

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
	[string]$OutputDir = ''
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

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
	$OutputDir = Join-Path $BuildDir $Config
}
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
$packageDir = Join-Path $outputDirPath $packageName
$zipPath = Join-Path $outputDirPath "$packageName.zip"

New-Item -ItemType Directory -Force $outputDirPath | Out-Null
if (Test-Path $packageDir) {
	Remove-Item -Recurse -Force $packageDir
}

New-Item -ItemType Directory -Force $packageDir | Out-Null
foreach ($d in @('assets', 'audio', 'mods')) {
	New-Item -ItemType Directory -Force (Join-Path $packageDir $d) | Out-Null
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
	Copy-Item $src $packageDir -Force
}

$readmeSrc = Join-Path $repoRoot 'README.md'
if (Test-Path $readmeSrc) {
	Copy-Item $readmeSrc (Join-Path $packageDir 'README.md') -Force
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
Copy-Item (Join-Path $assetsSrc '*') (Join-Path $packageDir 'assets') -Recurse -Force

# Audio + mods are copied from build\<Config>\...
$audioSrc = Join-Path $buildOutputDir 'audio'
$modsSrc = Join-Path $buildOutputDir 'mods'
if (-not (Test-Path $audioSrc)) { throw "Audio directory not found: $audioSrc" }
if (-not (Test-Path $modsSrc)) { throw "Mods directory not found: $modsSrc" }
Copy-Item (Join-Path $audioSrc '*') (Join-Path $packageDir 'audio') -Recurse -Force
Copy-Item (Join-Path $modsSrc '*') (Join-Path $packageDir 'mods') -Recurse -Force

if (Test-Path $zipPath) {
	Remove-Item -Force $zipPath
}
Compress-Archive -Path (Join-Path $packageDir '*') -DestinationPath $zipPath -Force

Write-Host "Release folder: $packageDir"
Write-Host "Release zip:    $zipPath"

