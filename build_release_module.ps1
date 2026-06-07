$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
try {
    cargo build --release -p variable-font-module
} finally {
    Pop-Location
}
