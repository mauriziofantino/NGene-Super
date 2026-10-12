# Native MSVC developer build: the same entry point is used by PowerShell and VS Code.
[CmdletBinding()]
param(
    [ValidateSet('Configure', 'Build', 'Test', 'Package')][string]$Action = 'Build',
    [ValidateSet('x64', 'ARM64')][string]$Architecture = 'x64',
    [ValidateSet('ON', 'OFF')][string]$Hackrf = 'ON',
    [switch]$MultiSdr,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [ValidateRange(1, 64)][int]$Parallel = 2,
    [string]$SourceDirectory = '',
    [switch]$ShowPlan
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([string]::IsNullOrWhiteSpace($SourceDirectory)) {
    $SourceDirectory = Split-Path -Path $PSScriptRoot -Parent
}
$SourceDirectory = [IO.Path]::GetFullPath($SourceDirectory)
. (Join-Path $PSScriptRoot 'windows-tools.ps1')
$configurePreset = 'windows-msvc-' + $Architecture.ToLowerInvariant()
$relativeBuild = 'build/msvc-' + $Architecture.ToLowerInvariant()
if ($MultiSdr) { $configurePreset += '-multisdr'; $relativeBuild += '-multisdr' }
elseif ($Hackrf -eq 'ON') { $configurePreset += '-hackrf'; $relativeBuild += '-hackrf' }
$buildPreset = $configurePreset
if ($Configuration -eq 'Debug') { $buildPreset += '-debug' }
$build = Join-Path $SourceDirectory $relativeBuild
$commands = @([pscustomobject]@{
    label = 'configure'; tool = 'cmake'; arguments = @('--preset', $configurePreset)
})
if ($Action -ne 'Configure') {
    $commands += [pscustomobject]@{
        label = 'build'; tool = 'cmake'; arguments = @('--build', '--preset', $buildPreset, '--parallel', "$Parallel")
    }
}
if ($Action -eq 'Test') {
    $commands += [pscustomobject]@{
        label = 'test'; tool = 'ctest'; arguments = @('--preset', $buildPreset, '--no-tests=error', '--output-on-failure',
            '--output-junit', (Join-Path $build "$Configuration-junit.xml"))
    }
}
if ($Action -eq 'Package') {
    $commands += [pscustomobject]@{
        label = 'package'; tool = 'cpack'; arguments = @('--config', (Join-Path $build 'CPackConfig.cmake'),
            '-C', $Configuration, '-B', (Join-Path $SourceDirectory "out/$buildPreset"))
    }
}
# Expose a read-only plan for testing preset selection on any host. This does not
# certify, run or emulate MSVC; real execution below always requires native Windows.
if ($ShowPlan) {
    [ordered]@{ configurePreset = $configurePreset; buildPreset = $buildPreset; commands = $commands } |
        ConvertTo-Json -Depth 8
    exit 0
}
if ($env:OS -ne 'Windows_NT') { throw 'This entry point requires native Windows/MSVC. Use the Linux/macOS CMake presets on those hosts.' }
if ($Action -eq 'Test') {
    $hostArchitecture = if ($env:PROCESSOR_ARCHITEW6432) { $env:PROCESSOR_ARCHITEW6432 } else { $env:PROCESSOR_ARCHITECTURE }
    $expectedHost = if ($Architecture -eq 'x64') { 'AMD64' } else { 'ARM64' }
    if ($hostArchitecture -ne $expectedHost) { throw 'Cross-builds are supported; runtime tests require a native target host.' }
}
$tools = Resolve-CMakeTools
Write-Host "Using CMake $($tools.version): $($tools.cmake)"
$logs = Join-Path $build 'logs'
$null = New-Item -ItemType Directory -Path $logs -Force
# Presets choose the installed Visual Studio automatically. Do not inherit a
# generator/toolset selected for an unrelated project in the parent shell.
$overrides = @('CMAKE_GENERATOR', 'CMAKE_GENERATOR_PLATFORM', 'CMAKE_GENERATOR_TOOLSET', 'CMAKE_GENERATOR_INSTANCE')
$savedEnvironment = @{}
foreach ($name in $overrides) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}
Push-Location $SourceDirectory
try {
    foreach ($step in $commands) {
        $executable = $tools.($step.tool)
        $arguments = $step.arguments
        $log = Join-Path $logs "$($step.label)-$Configuration.log"
        Write-Host "[$($step.label)] $executable $($arguments -join ' ')"
        $savedPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & $executable @arguments 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $log | Out-Host
            $exitCode = $LASTEXITCODE
        } finally { $ErrorActionPreference = $savedPreference }
        if ($exitCode -ne 0) { throw "$($step.label) failed (exit $exitCode). Full log: $log" }
        $diagnostics = @(Select-String -LiteralPath $log -Pattern '(?i)\b(?:C|D|A|LNK|MSB)\d{4}\s*:|CMake Warning')
        if ($diagnostics.Count -ne 0) { throw "$($step.label) emitted diagnostics that require correction. Full log: $log" }
        if ($step.label -eq 'configure') {
            $cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw
            if ($cache -notmatch 'CMAKE_GENERATOR:INTERNAL=Visual Studio') { throw 'The native MSVC presets require the installed Visual Studio generator.' }
            foreach ($target in @('ngene', 'ngene_core', 'ngene_control')) {
                Assert-VsWarningPolicy (Join-Path $build "$target.vcxproj") $false
            }
            Assert-VsWarningPolicy (Join-Path $build 'ngene_http.vcxproj') $true
            $hackrfProject = Join-Path $build 'ngene_hackrf_native.vcxproj'
            if (Test-Path -LiteralPath $hackrfProject) { Assert-VsWarningPolicy $hackrfProject $true }
        }
    }
    Write-Host "Completed $Action ($Architecture, HackRF=$Hackrf, $Configuration). Logs: $logs"
} finally {
    Pop-Location
    foreach ($name in $overrides) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
}
