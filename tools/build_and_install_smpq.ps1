<#
.SYNOPSIS
    Downloads, builds and installs SMPQ from source on Windows.

.DESCRIPTION
    Installs SMPQ to $env:LOCALAPPDATA\Programs\smpq for development on Windows.
    Requires: cmake, tar and Visual Studio as setup by following the DevilutionX build instructions
#>

$ErrorActionPreference = "Stop"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake is required but not found in PATH."
}

if (-not (Get-Command tar -ErrorAction SilentlyContinue)) {
    Write-Error "tar is required but not found in PATH."
}

$STORMLIB_VERSION = "e87c2389cd7ed7e3bef4965a482f43cdddcf8f75"
$SMPQ_VERSION = "1.7"

$ScriptRoot = $PSScriptRoot
if (-not $ScriptRoot) { $ScriptRoot = $PWD }
$WorkDir = Join-Path $ScriptRoot "tmp\smpq"
$StormLib_Src = Join-Path $WorkDir "StormLib-$STORMLIB_VERSION"
$StagingDir = Join-Path $WorkDir "smpq-staging"
$StagingDirFwd = $StagingDir -replace "\\", "/"
$Smpq_Src = Join-Path $WorkDir "smpq-$SMPQ_VERSION"
$Smpq_BuildDir = Join-Path $Smpq_Src "build"
$InstallDir = "$env:LOCALAPPDATA\Programs\smpq"

if (-not (Test-Path -Path $WorkDir)) {
    New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null
}

$StormLib_Tar = Join-Path $WorkDir "stormlib-${STORMLIB_VERSION}.tar.gz"
if (-not (Test-Path -Path $StormLib_Tar -PathType Leaf)) {
    Write-Host "Downloading StormLib ${STORMLIB_VERSION}..."
    $StormLib_Url = "https://github.com/ladislav-zezula/StormLib/archive/${STORMLIB_VERSION}.tar.gz"
    Invoke-WebRequest -Uri $StormLib_Url -OutFile $StormLib_Tar
}
else {
    Write-Host "StormLib tarball already exists. Skipping download."
}

if (-not (Test-Path -Path $StormLib_Src)) {
    Write-Host "Extracting StormLib ${STORMLIB_VERSION}..."
    tar -C $WorkDir -xf $StormLib_Tar
}
else {
    Write-Host "StormLib source already exists. Skipping extraction."
}

Write-Host "Building and installing StormLib ${STORMLIB_VERSION}..."
$StormLib_BuildDir = Join-Path $StormLib_Src "build"
cmake -S "$StormLib_Src" -B "$StormLib_BuildDir" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_INSTALL_PREFIX="$StagingDir" `
    -DBUILD_SHARED_LIBS=OFF `
    -DSTORM_BUILD_TESTS=OFF `
    -DSTORM_USE_BUNDLED_LIBRARIES=ON `
    -DCMAKE_C_FLAGS="/wd4101 /wd4244 /wd4267 /wd4334" `
    -DCMAKE_CXX_FLAGS="/wd4101 /wd4244 /wd4267 /wd4334"
cmake --build "$StormLib_BuildDir" --config Release --target install

$Smpq_Tar = Join-Path $WorkDir "smpq_${SMPQ_VERSION}.orig.tar.gz"
if (-not (Test-Path -Path $Smpq_Tar)) {
    Write-Host "Downloading SMPQ ${SMPQ_VERSION}..."
    $Smpq_Url = "https://launchpad.net/smpq/trunk/${SMPQ_VERSION}/+download/smpq_${SMPQ_VERSION}.orig.tar.gz"
    Invoke-WebRequest -Uri $Smpq_Url -OutFile $Smpq_Tar
}
else {
    Write-Host "SMPQ tarball already exists. Skipping download."
}

if (-not (Test-Path -Path $Smpq_Src)) {
    Write-Host "Extracting SMPQ ${SMPQ_VERSION}..."
    tar -C $WorkDir -xf $Smpq_Tar
}
else {
    Write-Host "SMPQ source already exists. Skipping extraction."
}

Write-Host "Applying patches to SMPQ..."
$CMakeListPath = Join-Path $Smpq_Src "CMakeLists.txt"
if (Test-Path $CMakeListPath) {
    $CMakeParams = Get-Content -Raw $CMakeListPath
    if ($CMakeParams -notmatch "PROPERTIES LANGUAGE CXX") {
        Write-Host "Patching CMakeLists.txt to force C++..."
        # StormLib is C++ and must be linked with a C++ linker.
        $Patch1 = "project(SMPQ)`r`nfile(GLOB_RECURSE CFILES `"`${CMAKE_SOURCE_DIR}/*.c`")`r`nSET_SOURCE_FILES_PROPERTIES(`${CFILES} PROPERTIES LANGUAGE CXX)"
        $CMakeParams = $CMakeParams -replace "project\(SMPQ\)", $Patch1
        
        # Do not generate the manual
        $CMakeParams = $CMakeParams -replace "if\(NOT CMAKE_CROSSCOMPILING\)", "if(FALSE)"
        Set-Content -Path $CMakeListPath -Value $CMakeParams -Encoding UTF8
    }
}

# Fix C++ compilation errors in dirname.c (implicit void* cast)
$DirnameC = Join-Path $Smpq_Src "dirname.c"
if (Test-Path $DirnameC) {
    $DirnameContent = Get-Content -Raw $DirnameC
    if ($DirnameContent -notmatch "\(char \*\) realloc") {
        Write-Host "Patching dirname.c to add explicit casts..."
        $DirnameContent = $DirnameContent -replace "retfail = realloc", "retfail = (char *) realloc"
        Set-Content -Path $DirnameC -Value $DirnameContent -Encoding UTF8
    }
}

Write-Host "Building and installing SMPQ ${SMPQ_VERSION}..."
cmake -S "$Smpq_Src" -B "$Smpq_BuildDir" `
    -DCMAKE_INSTALL_PREFIX="$InstallDir" `
    -DWITH_KDE=OFF `
    -DCMAKE_PREFIX_PATH="$StagingDirFwd" `
    -DSTORMLIB_INCLUDE_DIR="$StagingDirFwd/include" `
    -DSTORMLIB_LIBRARY="$StagingDirFwd/lib/StormLib.lib" `
    -DCMAKE_CXX_FLAGS="-D__STORMLIB_NO_STATIC_LINK__ /wd4312" `
    -DCMAKE_C_FLAGS="-D__STORMLIB_NO_STATIC_LINK__ /wd4312"
cmake --build "$Smpq_BuildDir" --config Release --target install
if ($LASTEXITCODE -eq 0) {
    Write-Host "SMPQ installed successfully."
    # Add to PATH
    $BinPath = Join-Path $InstallDir "bin"
    $UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($UserPath -notlike "*$BinPath*") {
        Write-Host "Adding '$BinPath' to User PATH environment variable..."
        [Environment]::SetEnvironmentVariable("Path", "$UserPath;$BinPath", "User")
        $env:Path += ";$BinPath"
    }
    else {
        Write-Host "'$BinPath' is already in the User PATH."
    }

    Write-Host "Done."
}
else {
    Write-Error "Failed to install SMPQ."
    exit 1
}

