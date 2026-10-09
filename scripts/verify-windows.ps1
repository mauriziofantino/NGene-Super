# Native MSVC validation, run from an unmodified checkout with VS 2026 and CMake >=4.2.
# No SDK paths, baseline update, file edits, manual DLL copies or MinGW are used.
param([ValidateSet('x64', 'ARM64')][string]$Architecture = 'x64',
      [string]$Generator = 'Visual Studio 18 2026')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
$source = Split-Path $PSScriptRoot -Parent
$build = Join-Path $source "build/zero-touch-$Architecture"
if (Test-Path $build) { throw 'Validation requires a new build directory.' }
# Clear optional overrides so that AUTO is exercised without a preconfigured HackRF SDK.
Remove-Item Env:HACKRF_ROOT, Env:HACKRF_SOURCE_DIR -ErrorAction SilentlyContinue
Invoke-Checked cmake @('-S', $source, '-B', $build, '-G', $Generator, '-A', $Architecture,
    '-DNGENE_WITH_HACKRF=ON', '-DNGENE_HACKRF_PROVIDER=AUTO',
    '-DNGENE_BOOTSTRAP_VCPKG=ON', '-DNGENE_WITH_WEB=ON',
    '-DNGENE_BUILD_TESTS=ON', '-DNGENE_REQUIRE_MSVC=ON')
foreach ($configuration in @('Debug', 'Release')) {
    Invoke-Checked cmake @('--build', $build, '--config', $configuration, '--parallel', '2', '--verbose')
    # The runner executes only host-native binaries. ARM64 cross builds are tested separately.
    if ($Architecture -eq 'x64' -or $env:PROCESSOR_ARCHITECTURE -eq 'ARM64') {
        Invoke-Checked ctest @('--test-dir', $build, '-C', $configuration, '--output-on-failure')
    }
}
$package = Join-Path $source "build/package-$Architecture"
Invoke-Checked cmake @('--install', $build, '--config', 'Release', '--prefix', $package)
Invoke-Checked cpack @('--config', (Join-Path $build 'CPackConfig.cmake'), '-C', 'Release', '-B', $package)
if ($Architecture -eq 'x64' -or $env:PROCESSOR_ARCHITECTURE -eq 'ARM64') {
    # Relocate and clear dependency PATH entries: the installed package must carry its DLL closure.
    $temporary = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
    $relocated = Join-Path $temporary "ngene-relocated-$([guid]::NewGuid())"
    $archives = @(Get-ChildItem $package -Filter '*.zip' -File)
    if ($archives.Count -ne 1) { throw 'Expected exactly one generated CPack ZIP.' }
    Expand-Archive -LiteralPath $archives[0].FullName -DestinationPath $relocated
    $executables = @(Get-ChildItem $relocated -Filter 'ngene.exe' -File -Recurse)
    if ($executables.Count -ne 1) { throw 'Expected exactly one NGene executable in the ZIP.' }
    $savedPath = $env:PATH
    try {
        $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
        Invoke-Checked $executables[0].FullName @('--version')
        Invoke-Checked $executables[0].FullName @('--help')
    } finally { $env:PATH = $savedPath }
    Remove-Item $relocated -Recurse
}
