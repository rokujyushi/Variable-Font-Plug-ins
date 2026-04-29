# Build VariableFontModule (Release x64)
$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
Set-Location $PSScriptRoot

if (Test-Path ".\obj") { Remove-Item ".\obj" -Recurse -Force }
if (Test-Path ".\bin") { Remove-Item ".\bin" -Recurse -Force }

msbuild ".\VariableFontModule.vcxproj" /p:Configuration=Release /p:Platform=x64

Pop-Location
