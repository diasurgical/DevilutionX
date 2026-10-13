# Removes all upstream GitHub Actions workflows from a local checkout of the
# VTSTech/DevilutionX fork, keeping ONLY gpu-shaders.yml (our GPU shader
# scaler CI). Run from the repo root in PowerShell, then commit + push.

$ErrorActionPreference = "Stop"
$keep = "gpu-shaders.yml"
$wf = Join-Path (Get-Location) ".github\workflows"

if (-not (Test-Path $wf)) {
    Write-Error "error: run this from inside the repository (no .github\workflows here)"
}

$removed = 0
Get-ChildItem -Path $wf -Include *.yml, *.yaml -File -Recurse | ForEach-Object {
    if ($_.Name -eq $keep) {
        Write-Host ("keep    " + $_.FullName)
    } else {
        Remove-Item -LiteralPath $_.FullName -Force
        Write-Host ("removed " + $_.FullName)
        $removed++
    }
}

Write-Host ""
Write-Host ("Done. Removed $removed workflow file(s).")
Write-Host "Now commit and push:"
Write-Host "    git commit -am 'CI: remove upstream workflows, keep GPU shader scaler pipeline'"
Write-Host "    git push"
