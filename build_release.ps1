$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
try {
    $command = Get-Command "au2" -ErrorAction SilentlyContinue
    $localCli = "I:\aviutl2-cli\au2-v0.8.3-windows.exe"
    if ($command) {
        & $command.Source develop --profile release --skip-start
    } elseif (Test-Path $localCli) {
        & $localCli develop --profile release --skip-start
    } else {
        throw "aviutl2-cli (au2) was not found."
    }
} finally {
    Pop-Location
}
