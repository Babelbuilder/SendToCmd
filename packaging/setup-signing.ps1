# Install only the Windows SDK signing tools; no certificate or private key is created.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$sdkBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
$tools = @(Get-ChildItem $sdkBin -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.Directory.Name -eq 'x64' })
if (-not $tools.Count) {
    $toolRoot = Join-Path $env:LOCALAPPDATA 'SendToCmd/SigningTools'
    New-Item -ItemType Directory -Path $toolRoot -Force | Out-Null
    $toolPath = Join-Path $toolRoot 'signtool.exe'
    if (-not (Test-Path -LiteralPath $toolPath)) {
        $packagePath = Join-Path $toolRoot 'sdk-buildtools.zip'
        Invoke-WebRequest 'https://api.nuget.org/v3-flatcontainer/microsoft.windows.sdk.buildtools/10.0.26100.7705/microsoft.windows.sdk.buildtools.10.0.26100.7705.nupkg' -OutFile $packagePath -UseBasicParsing
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip = [System.IO.Compression.ZipFile]::OpenRead($packagePath)
        try {
            $zip.Entries | Where-Object { $_.FullName -match '/x64/(signtool.exe|.*\.dll)$' } |
                ForEach-Object { [System.IO.Compression.ZipFileExtensions]::ExtractToFile($_, (Join-Path $toolRoot $_.Name), $true) }
        } finally { $zip.Dispose() }
    }
    $signature = Get-AuthenticodeSignature -FilePath $toolPath
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation(?:,|$)') {
        throw 'Downloaded SignTool must have a valid Microsoft Authenticode signature.'
    }
    $tools = @(Get-Item -LiteralPath $toolPath)
}
if (-not $tools.Count) { throw 'SignTool was not found after installation.' }
$tools | ForEach-Object { Write-Host "SignTool: $($_.FullName)" }
foreach ($location in @('CurrentUser', 'LocalMachine')) {
    $certificates = @(Get-ChildItem "Cert:/$location/My" -CodeSigningCert |
        Where-Object { $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) })
    Write-Host "$location code signing certificates: $($certificates.Count)"
    $certificates | Select-Object Subject, Thumbprint, NotAfter
}
Write-Host 'Use a publicly trusted CA certificate or Azure Artifact Signing Public Trust before signing a release.'
