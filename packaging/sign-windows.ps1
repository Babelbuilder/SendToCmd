<#
Sign an already-deployed portable Windows folder, then create a signed ZIP.
The private key stays in the Windows certificate store or Artifact Signing.
Do not put a PFX file or password in this repository or in command arguments.
#>
[CmdletBinding(DefaultParameterSetName = 'Store')]
param(
    [Parameter(Mandatory = $true)] [string]$PackageDirectory,
    [Parameter(Mandatory = $true, ParameterSetName = 'Store')] [string]$CertificateThumbprint,
    [Parameter(Mandatory = $true, ParameterSetName = 'Artifact')] [string]$ArtifactSigningDlib,
    [Parameter(Mandatory = $true, ParameterSetName = 'Artifact')] [string]$ArtifactSigningMetadata,
    [string]$OutputZip = '',
    [ValidateSet('CurrentUser', 'LocalMachine')] [string]$CertificateStoreLocation = 'CurrentUser',
    [string]$SignToolPath = '',
    [string]$TimestampUrl = ''
)

$ErrorActionPreference = 'Stop'
$package = (Resolve-Path $PackageDirectory).Path
if (-not (Test-Path (Join-Path $package 'SendToCmd.exe'))) {
    throw 'The package folder must contain SendToCmd.exe.'
}
if (-not (Test-Path (Join-Path $package 'plugins/platforms/qwindows.dll'))) {
    throw 'The portable package is missing qwindows.dll.'
}
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
    throw 'SignTool is missing. Install the Windows SDK Signing Tools or pass -SignToolPath.'
}
$SignToolPath = (Resolve-Path -LiteralPath $SignToolPath).Path
if (-not $TimestampUrl) {
    $TimestampUrl = if ($PSCmdlet.ParameterSetName -eq 'Artifact') {
        'http://timestamp.acs.microsoft.com'
    } else {
        'http://timestamp.digicert.com'
    }
}
if ($PSCmdlet.ParameterSetName -eq 'Artifact') {
    $ArtifactSigningDlib = (Resolve-Path $ArtifactSigningDlib).Path
    $ArtifactSigningMetadata = (Resolve-Path $ArtifactSigningMetadata).Path
} else {
    $CertificateThumbprint = $CertificateThumbprint -replace '\s', ''
    if ($CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') {
        throw 'CertificateThumbprint must be the 40-digit SHA-1 thumbprint from the Windows certificate store.'
    }
    $certificate = Get-Item "Cert:/$CertificateStoreLocation/My/$CertificateThumbprint" -ErrorAction Stop
    if (-not $certificate.HasPrivateKey) { throw 'The signing certificate has no accessible private key.' }
    if ($certificate.NotAfter -le (Get-Date) -or $certificate.NotBefore -gt (Get-Date)) {
        throw 'The signing certificate is expired or not yet valid.'
    }
    if ('1.3.6.1.5.5.7.3.3' -notin @($certificate.EnhancedKeyUsageList.ObjectId.Value)) {
        throw 'The certificate must have the Code Signing extended key usage.'
    }
    if ($certificate.Subject -eq $certificate.Issuer) {
        throw 'A self-signed certificate cannot satisfy public Smart App Control trust.'
    }
    $chain = New-Object System.Security.Cryptography.X509Certificates.X509Chain
    try {
        if (-not $chain.Build($certificate)) { throw 'The signing certificate chain is not trusted or failed revocation checks.' }
    } finally { $chain.Dispose() }
}

$binaries = @(Get-ChildItem $package -Recurse -File | Where-Object { $_.Extension -in '.exe', '.dll' })
if ($binaries.Count -eq 0) { throw 'No Windows binaries found in the package.' }
foreach ($binary in $binaries) {
    $signature = Get-AuthenticodeSignature -FilePath $binary.FullName
    if ($signature.Status -eq 'Valid') {
        Write-Host "Already trusted: $($binary.Name)"
        continue
    }
    if ($PSCmdlet.ParameterSetName -eq 'Artifact') {
        & $SignToolPath sign /v /fd SHA256 /tr $TimestampUrl /td SHA256 /dlib $ArtifactSigningDlib /dmdf $ArtifactSigningMetadata $binary.FullName
    } else {
        $storeArguments = @()
        if ($CertificateStoreLocation -eq 'LocalMachine') { $storeArguments += '/sm' }
        & $SignToolPath sign /v /fd SHA256 /tr $TimestampUrl /td SHA256 /sha1 $CertificateThumbprint @storeArguments $binary.FullName
    }
    if ($LASTEXITCODE -ne 0) { throw "Signing failed: $($binary.FullName)" }
}

foreach ($binary in $binaries) {
    & $SignToolPath verify /pa /all /v $binary.FullName
    if ($LASTEXITCODE -ne 0) { throw "Signature verification failed: $($binary.FullName)" }
}

if (-not $OutputZip) {
    $OutputZip = Join-Path (Split-Path -Parent $package) 'SendToCmd-2.0-Windows-x64-signed.zip'
}
if (Test-Path $OutputZip) { Remove-Item $OutputZip -Force }
Compress-Archive -Path $package -DestinationPath $OutputZip -CompressionLevel Optimal
$hash = (Get-FileHash -Path $OutputZip -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $(Split-Path -Leaf $OutputZip)" | Set-Content "$OutputZip.sha256" -Encoding ASCII
Write-Host "Signed portable package: $OutputZip"
Write-Host "SHA256: $hash"
