# Sign the standalone EXE on Windows. Requires a trusted code-signing identity.
[CmdletBinding(DefaultParameterSetName = 'Store')]
param(
    [string]$InputFile = '',
    [string]$OutputFile = '',
    [Parameter(Mandatory = $true, ParameterSetName = 'Store')] [string]$CertificateThumbprint,
    [Parameter(Mandatory = $true, ParameterSetName = 'Artifact')] [string]$ArtifactSigningDlib,
    [Parameter(Mandatory = $true, ParameterSetName = 'Artifact')] [string]$ArtifactSigningMetadata,
    [ValidateSet('CurrentUser', 'LocalMachine')] [string]$CertificateStoreLocation = 'CurrentUser',
    [string]$SignToolPath = '',
    [string]$TimestampUrl = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $InputFile) { $InputFile = Join-Path $root 'dist/SendToCmd-2.0-Windows-x64-static.exe' }
if (-not $OutputFile) { $OutputFile = Join-Path $root 'dist/SendToCmd-2.0-Windows-x64-static-signed.exe' }
$source = (Resolve-Path -LiteralPath $InputFile).Path
$destination = [System.IO.Path]::GetFullPath($OutputFile)
if ($source -eq $destination) { throw 'Use a separate output file so the unsigned original remains available.' }

if (-not $SignToolPath) {
    $command = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($command) { $SignToolPath = $command.Source }
    else {
        $sdkBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
        $SignToolPath = Get-ChildItem $sdkBin -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^10\.0\.\d+\.\d+$' } |
            Sort-Object { [version]$_.Name } -Descending |
            ForEach-Object { Join-Path $_.FullName 'x64/signtool.exe' } |
            Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
}
if (-not $SignToolPath) {
    $localTool = Join-Path $env:LOCALAPPDATA 'SendToCmd/SigningTools/signtool.exe'
    if (Test-Path -LiteralPath $localTool) { $SignToolPath = $localTool }
}
if (-not $SignToolPath -or -not (Test-Path -LiteralPath $SignToolPath)) {
    throw 'SignTool is missing. Install Windows SDK Signing Tools or run packaging/setup-signing.ps1.'
}
$SignToolPath = (Resolve-Path -LiteralPath $SignToolPath).Path

if ($PSCmdlet.ParameterSetName -eq 'Store') {
    $CertificateThumbprint = $CertificateThumbprint -replace '\s', ''
    if ($CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') {
        throw 'CertificateThumbprint must be the 40-digit SHA-1 thumbprint from the Windows certificate store.'
    }
    $certificate = Get-Item "Cert:/$CertificateStoreLocation/My/$CertificateThumbprint" -ErrorAction Stop
    if (-not $certificate.HasPrivateKey) { throw 'The certificate has no accessible private key.' }
    if ($certificate.NotAfter -le (Get-Date) -or $certificate.NotBefore -gt (Get-Date)) {
        throw 'The certificate is expired or not yet valid.'
    }
    if ('1.3.6.1.5.5.7.3.3' -notin @($certificate.EnhancedKeyUsageList.ObjectId.Value)) {
        throw 'The certificate must have the Code Signing extended key usage.'
    }
    if ($certificate.Subject -eq $certificate.Issuer) {
        throw 'A self-signed certificate does not provide public Smart App Control trust.'
    }
    if (-not $TimestampUrl) { $TimestampUrl = 'http://timestamp.digicert.com' }
    $storeArguments = @()
    if ($CertificateStoreLocation -eq 'LocalMachine') { $storeArguments += '/sm' }
    $signArguments = @('sign', '/v', '/fd', 'SHA256', '/tr', $TimestampUrl,
                       '/td', 'SHA256', '/sha1', $CertificateThumbprint) + $storeArguments
} else {
    $ArtifactSigningDlib = (Resolve-Path -LiteralPath $ArtifactSigningDlib).Path
    $ArtifactSigningMetadata = (Resolve-Path -LiteralPath $ArtifactSigningMetadata).Path
    if (-not $TimestampUrl) { $TimestampUrl = 'http://timestamp.acs.microsoft.com' }
    $signArguments = @('sign', '/v', '/fd', 'SHA256', '/tr', $TimestampUrl,
                       '/td', 'SHA256', '/dlib', $ArtifactSigningDlib,
                       '/dmdf', $ArtifactSigningMetadata)
}

New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
Copy-Item -LiteralPath $source -Destination $destination -Force
& $SignToolPath @signArguments $destination
if ($LASTEXITCODE -ne 0) {
    Remove-Item -LiteralPath $destination -Force
    throw 'Code signing failed.'
}
& $SignToolPath verify /pa /all /v $destination
if ($LASTEXITCODE -ne 0) { throw 'Signature verification failed.' }
$hash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $(Split-Path -Leaf $destination)" | Set-Content -LiteralPath "$destination.sha256" -Encoding ASCII
Write-Host "Signed EXE: $destination"
Write-Host "SHA256: $hash"
