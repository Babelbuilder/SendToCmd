param(
    [string]$BuildDirectory = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $root 'build-windows-native' }
$distRoot = Join-Path $root 'dist'
$package = Join-Path $distRoot 'SendToCmd-2.0-Windows-x64'
$archive = Join-Path $distRoot 'SendToCmd-2.0-Windows-x64-unsigned.zip'

cmake -S $root -B $BuildDirectory -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed. Use a Windows Qt 6 MSVC/MinGW build environment.' }
cmake --build $BuildDirectory --config Release
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed.' }

$candidates = @(
    (Join-Path $BuildDirectory 'Release/SendToCmd.exe'),
    (Join-Path $BuildDirectory 'SendToCmd.exe')
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) { throw 'SendToCmd.exe was not found in the build directory.' }

if (Test-Path $package) { Remove-Item $package -Recurse -Force }
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item $exe (Join-Path $package 'SendToCmd.exe')
Copy-Item (Join-Path $root 'examples.txt') $package
Copy-Item (Join-Path $root 'README.md') $package

$deploy = Get-Command windeployqt.exe -ErrorAction Stop
& $deploy.Source --release --compiler-runtime --dir $package (Join-Path $package 'SendToCmd.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed.' }

# Qt must find its platform plugin next to the portable executable.
@('[Paths]', 'Plugins=plugins') | Set-Content (Join-Path $package 'qt.conf') -Encoding ASCII
if (-not (Test-Path (Join-Path $package 'plugins/platforms/qwindows.dll'))) {
    throw 'qwindows.dll is missing from the portable package.'
}
$cache = Get-Content (Join-Path $BuildDirectory 'CMakeCache.txt') -Raw
if ($cache -match 'CMAKE_CXX_COMPILER:[^\r\n]*=[^\r\n]*(g\+\+|mingw)') {
    foreach ($runtime in @('libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll')) {
        if (-not (Test-Path (Join-Path $package $runtime))) {
            throw "MinGW runtime is missing: $runtime. Use the matching Qt/MinGW build environment."
        }
    }
} else {
    Write-Warning 'An MSVC build may still require the Visual C++ Redistributable on a clean target PC. Use the matching Qt MinGW toolchain for a no-install portable folder.'
}

if (Test-Path $archive) { Remove-Item $archive -Force }
Compress-Archive -Path $package -DestinationPath $archive -CompressionLevel Optimal
Write-Host "Unsigned portable package: $archive"
Write-Host 'Extract the whole folder before running; signing instructions are in README.md.'
