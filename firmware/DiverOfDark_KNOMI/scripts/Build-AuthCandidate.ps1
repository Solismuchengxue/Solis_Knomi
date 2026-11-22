[CmdletBinding()]
param(
    [string]$PlatformIOPython = 'D:\PlatformIO_Core\penv\Scripts\python.exe',
    [switch]$BuildWebUi
)
$ErrorActionPreference = 'Stop'
$firmwareRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$candidateRoot = Join-Path $firmwareRoot '.pio\build-basic-auth-20261122'
$reviewRoot = Join-Path $firmwareRoot '.pio\http-basic-auth-review-20261122'
$null = New-Item -ItemType Directory -Path $reviewRoot -Force
$logPath = Join-Path $reviewRoot 'build.log'
$manifestPath = Join-Path $reviewRoot 'candidate-manifest.json'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $firmwareRoot '..\..'))

function Get-Evidence([string]$LiteralPath) {
    if (Test-Path -LiteralPath $LiteralPath -PathType Leaf) {
        $item = Get-Item -LiteralPath $LiteralPath
        return @{ path = $item.FullName; bytes = $item.Length; sha256 = (Get-FileHash -LiteralPath $LiteralPath -Algorithm SHA256).Hash }
    }
}
$oldWebUi = Join-Path $reviewRoot 'webui-before\knomiWebpage.h'
if ($BuildWebUi) {
    $webUiHeader = Join-Path $firmwareRoot 'src\generated\knomiWebpage.h'
    if (-not (Test-Path -LiteralPath $oldWebUi)) {
        $null = New-Item -ItemType Directory -Path (Split-Path -Parent $oldWebUi) -Force
        Copy-Item -LiteralPath $webUiHeader -Destination $oldWebUi
        if ((Get-FileHash -LiteralPath $webUiHeader).Hash -ne (Get-FileHash -LiteralPath $oldWebUi).Hash) {
            throw 'WebUI recovery copy verification failed.'
        }
    }
    $uiLog = Join-Path $reviewRoot 'webui-build.log'
    Push-Location (Join-Path $firmwareRoot 'ui-ng')
    try {
        & 'D:\Nodejs\npm.cmd' run build *> $uiLog
        $uiExit = $LASTEXITCODE
    } finally { Pop-Location }
    Get-Content -LiteralPath $uiLog -Tail 12
    if ($uiExit -ne 0) { throw ('WebUI build failed: ' + $uiLog) }
}
$protectedPaths = @(
    (Join-Path $firmwareRoot 'src\network\KlipperStreaming.h'),
    (Join-Path $firmwareRoot 'partition.csv'),
    (Join-Path $firmwareRoot 'boards\esp32r8_4mb.json')
)
foreach ($directory in @('src\scenes', 'src\generated', 'ui-ng\src')) {
    $protectedPaths += @(Get-ChildItem -LiteralPath (Join-Path $firmwareRoot $directory) -File -Recurse | Select-Object -ExpandProperty FullName)
}
$before = @($protectedPaths | ForEach-Object { Get-Evidence $_ })
$oldArtifacts = @(
    (Join-Path $firmwareRoot '.pio\build\knomiv1-4mb\firmware.bin'),
    (Join-Path $repoRoot 'esp_flash_download_tool\bin\firmware.bin'),
    (Join-Path $repoRoot 'release\flash_tool_v1_0_1.zip'),
    $oldWebUi
)
$recovery = @($oldArtifacts | ForEach-Object { Get-Evidence $_ })
$head = & git -C $repoRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the source baseline.' }
$baseline = @{ baseline_head = $head; protected_before = $before; recovery_untouched = $recovery }
$baseline | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding utf8NoBOM
$previousBuildDir = $env:PLATFORMIO_BUILD_DIR
try {
    $env:PLATFORMIO_BUILD_DIR = $candidateRoot
    & $PlatformIOPython -m platformio run --project-dir $firmwareRoot --environment knomiv1-4mb *> $logPath
    $buildExit = $LASTEXITCODE
} finally {
    $env:PLATFORMIO_BUILD_DIR = $previousBuildDir
}
Get-Content -LiteralPath $logPath -Tail 35
if ($buildExit -ne 0) { throw ('Candidate build failed. Log: ' + $logPath) }
foreach ($evidence in @($before) + @($recovery)) {
    if ((Get-FileHash -LiteralPath $evidence.path -Algorithm SHA256).Hash -ne $evidence.sha256) {
        throw ('Protected source/resource/recovery artifact changed: ' + $evidence.path)
    }
}
$artifacts = @(Get-ChildItem -LiteralPath (Join-Path $candidateRoot 'knomiv1-4mb') -File |
    Where-Object { $_.Extension -in @('.bin', '.elf', '.map') } | ForEach-Object { Get-Evidence $_.FullName })
$baseline['artifacts'] = $artifacts
$authSources = @('src\network\HttpBasicAuth.cpp', 'src\network\HttpBasicAuth.h', 'src\network\pages\ApiAuth.h',
    'src\network\pages\AbstractPage.h', 'src\network\pages\WebsocketLog.h', 'src\network\KnomiWebServer.h',
    'ui-ng\src\App.svelte', 'ui-ng\src\components\Password.svelte', 'platformio.ini', 'scripts\http_auth_build.py')
$baseline['auth_source_hashes'] = @($authSources | ForEach-Object { Get-Evidence (Join-Path $firmwareRoot $_) })
$baseline['protected_unchanged'] = $true
$baseline['build_exit'] = $buildExit
$baseline['hardware_verified'] = $false
$baseline['device_contacted'] = $false
$baseline | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding utf8NoBOM
Write-Output ('Candidate manifest: ' + $manifestPath)
Write-Output ('Protected source/resources unchanged: ' + $before.Count)
foreach ($artifact in $artifacts) {
    Write-Output ($artifact.path + ' bytes=' + $artifact.bytes + ' sha256=' + $artifact.sha256)
}
