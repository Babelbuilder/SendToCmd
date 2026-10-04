param(
    [Parameter(Mandatory = $true)]
    [string]$QtStaticPrefix,
    [string]$BuildDirectory = '',
    [string]$OutputFile = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $root 'build-windows-static' }
if (-not $OutputFile) { $OutputFile = Join-Path $root 'dist/SendToCmd-2.0-Windows-x64-static.exe' }

$qtConfig = Join-Path $QtStaticPrefix 'lib/cmake/Qt6/Qt6Config.cmake'
if (-not (Test-Path $qtConfig)) {
    throw "Static Qt CMake package not found: $qtConfig"
}

cmake -S $root -B $BuildDirectory `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$QtStaticPrefix" `
    -DSENDTOCMD_STATIC_WINDOWS=ON `
    -DBUILD_TESTING=OFF
if ($LASTEXITCODE -ne 0) { throw 'Static Windows CMake configuration failed.' }

cmake --build $BuildDirectory --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Static Windows build failed.' }

$candidates = @(
    (Join-Path $BuildDirectory 'Release/SendToCmd.exe'),
    (Join-Path $BuildDirectory 'SendToCmd.exe')
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) { throw 'SendToCmd.exe was not found.' }

$outputDirectory = Split-Path -Parent $OutputFile
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
Copy-Item $exe $OutputFile -Force
$strip = Get-Command strip.exe -ErrorAction SilentlyContinue
if ($strip) {
    & $strip.Source --strip-unneeded $OutputFile
    if ($LASTEXITCODE -ne 0) { throw 'Could not strip the standalone EXE.' }
}

$objdump = Get-Command objdump.exe -ErrorAction Stop
$imports = @(& $objdump.Source -p $OutputFile | Select-String -Pattern '^\s*DLL Name:\s*(.+)$' | ForEach-Object { $_.Matches[0].Groups[1].Value })
if ($LASTEXITCODE -ne 0 -or $imports.Count -eq 0) { throw 'Could not inspect EXE imports.' }
$systemDlls = @(
    'advapi32.dll', 'authz.dll', 'bcrypt.dll', 'comctl32.dll', 'comdlg32.dll', 'crypt32.dll', 'd3d11.dll', 'd3d12.dll', 'd3d9.dll',
    'dwmapi.dll', 'dwrite.dll', 'dxgi.dll', 'gdi32.dll', 'imm32.dll',
    'kernel32.dll', 'msvcrt.dll', 'ucrtbase.dll', 'netapi32.dll', 'ole32.dll', 'oleaut32.dll',
    'setupapi.dll', 'shcore.dll', 'shell32.dll', 'shlwapi.dll', 'user32.dll', 'userenv.dll',
    'uxtheme.dll', 'version.dll', 'winmm.dll', 'ws2_32.dll', 'wtsapi32.dll'
)
$unexpected = @($imports | Where-Object {
    $name = $_.ToLowerInvariant()
    $name -notin $systemDlls -and $name -notmatch '^(api-ms-win-|ext-ms-win-)'
})
if ($unexpected.Count) { throw "EXE still imports non-system DLLs: $($unexpected -join ', ')" }

Write-Host "Standalone unsigned EXE: $OutputFile"
Write-Host "Imported Windows DLLs: $($imports -join ', ')"
Write-Host 'Code signing is separate; an unsigned EXE may still be blocked by Windows Smart App Control.'
