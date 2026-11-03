function Ensure-VerifiedJunction {
    param(
        [Parameter(Mandatory = $true)] [string] $LinkPath,
        [Parameter(Mandatory = $true)] [string] $TargetPath
    )

    $target = [System.IO.Path]::GetFullPath($TargetPath).TrimEnd('\')
    $existing = Get-Item -LiteralPath $LinkPath -Force -ErrorAction SilentlyContinue
    if ($existing) {
        if ($existing.LinkType -ne 'Junction') {
            throw "Refusing to replace unexpected non-junction at $LinkPath"
        }

        $current = @($existing.Target) | Select-Object -First 1
        if (-not [string]::IsNullOrWhiteSpace($current)) {
            $current = [System.IO.Path]::GetFullPath($current).TrimEnd('\')
        }
        if ($current -eq $target) {
            return
        }

        Remove-Item -LiteralPath $LinkPath -Force
        if (Test-Path -LiteralPath $LinkPath) {
            throw "Unable to remove stale junction at $LinkPath"
        }
    }

    New-Item -ItemType Directory -Force -Path (Split-Path $LinkPath) | Out-Null
    New-Item -ItemType Junction -Path $LinkPath -Target $target | Out-Null
}
