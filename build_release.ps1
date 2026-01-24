# Build VariableFont (Release x64)
$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
Set-Location $PSScriptRoot

# Build VariableFont
msbuild ".\VariableFont.vcxproj" /p:Configuration=Release /p:Platform=x64

Pop-Location
