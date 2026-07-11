$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$downloadUrl = "https://github.com/g-truc/glm/archive/refs/tags/1.0.3.zip"
$tempZip = Join-Path $env:TEMP "glm-1.0.3.zip"
$tempFolder = Join-Path $env:TEMP "glm-1.0.3-extraido"
$targetFolder = Join-Path $projectRoot "include\glm"

Write-Host "Descargando GLM 1.0.3 desde el repositorio oficial..."
Invoke-WebRequest -Uri $downloadUrl -OutFile $tempZip

if (Test-Path $tempFolder) {
    Remove-Item $tempFolder -Recurse -Force
}

Expand-Archive -Path $tempZip -DestinationPath $tempFolder -Force
$sourceFolder = Join-Path $tempFolder "glm-1.0.3\glm"

if (-not (Test-Path $sourceFolder)) {
    throw "No se encontro la carpeta glm dentro del archivo descargado."
}

if (Test-Path $targetFolder) {
    Remove-Item $targetFolder -Recurse -Force
}

Copy-Item $sourceFolder $targetFolder -Recurse -Force

$licenseSource = Join-Path $tempFolder "glm-1.0.3\copying.txt"
$licenseTarget = Join-Path $projectRoot "include\GLM_LICENSE.txt"

if (Test-Path $licenseSource) {
    Copy-Item $licenseSource $licenseTarget -Force
}

Remove-Item $tempZip -Force
Remove-Item $tempFolder -Recurse -Force

Write-Host "GLM fue instalado correctamente en: $targetFolder"
