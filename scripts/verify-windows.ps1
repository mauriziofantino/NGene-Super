# Native Windows acceptance test. Logs and a JSON result are retained even on failure.
# The current committed revision is cloned into a new tree; no SDK or DLL preparation.
[CmdletBinding()]
param(
    [ValidateSet('x64', 'ARM64')][string]$Architecture = 'x64',
    [string]$SourceRepository = (Split-Path $PSScriptRoot -Parent),
    [string]$Revision = '',
    [string]$WorkRoot = (Join-Path ([IO.Path]::GetTempPath()) "ngene-msvc-$([guid]::NewGuid())")
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:OS -ne 'Windows_NT') { throw 'Native Windows/MSVC is required. Linux, WSL and Wine do not qualify.' }
$hostArchitecture = if ($env:PROCESSOR_ARCHITEW6432) { $env:PROCESSOR_ARCHITEW6432 } else { $env:PROCESSOR_ARCHITECTURE }
$expectedHost = if ($Architecture -eq 'x64') { 'AMD64' } else { 'ARM64' }
if ($hostArchitecture -ne $expectedHost) { throw 'Use a native target host for acceptance tests. Cross-builds use the ARM64 CMake presets.' }
foreach ($program in @('git', 'cmake', 'ctest', 'cpack')) { $null = Get-Command $program -ErrorAction Stop }
$WorkRoot = [IO.Path]::GetFullPath($WorkRoot)
if (Test-Path -LiteralPath $WorkRoot) { throw 'WorkRoot must not already exist: acceptance requires a fresh clone and build trees.' }
$null = New-Item -ItemType Directory -Path $WorkRoot
$logs = Join-Path $WorkRoot 'logs'
$null = New-Item -ItemType Directory -Path $logs
$checkout = Join-Path $WorkRoot 'checkout'
$script:commandNumber = 0
$script:evidence = [ordered]@{
    acceptance = 'pending'; platform = 'Windows'; architecture = $Architecture
    source = $SourceRepository; commit = ''; integratedVcpkgVerified = $false
    commands = @(); configurations = @(); failure = ''
}
function Invoke-Checked {
    param([string]$Label, [string]$Program, [string[]]$Arguments)
    $script:commandNumber++
    $log = Join-Path $logs ('{0:D3}-{1}.log' -f $script:commandNumber, $Label)
    Write-Host "[$Label] $Program $($Arguments -join ' ')"
    $savedPreference = $ErrorActionPreference
    try {
        # Native stderr is data for the log, including Windows PowerShell 5.1 ErrorRecords.
        $ErrorActionPreference = 'Continue'
        & $Program @Arguments 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $log | Out-Host
        $exitCode = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
    $warnings = @(Select-String -Path $log -Pattern '(?i)\b(?:C|D|A|LNK|MSB)\d{4}\s*:|CMake Warning')
    $script:evidence.commands += [ordered]@{
        label = $Label; program = $Program; arguments = $Arguments; log = $log
        exitCode = $exitCode; warnings = $warnings.Count
    }
    if ($exitCode -ne 0) { throw "$Label failed (exit $exitCode). Full log: $log" }
    if ($warnings.Count -ne 0) { throw "$Label emitted diagnostics that require correction. Full log: $log" }
}
function Invoke-Smoke {
    param([string]$Label, [string]$Executable, [bool]$Hackrf)
    if (-not (Test-Path -LiteralPath $Executable)) { throw "Missing runtime executable: $Executable" }
    $savedPath = $env:PATH
    try {
        # App-local DLLs must suffice; no dependency, compiler or SDK directory on PATH.
        $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
        Invoke-Checked "$Label-help" $Executable @('--help')
        Invoke-Checked "$Label-version" $Executable @('--version')
        if ($Hackrf) { Invoke-Checked "$Label-devices" $Executable @('--list-devices') }
    } finally { $env:PATH = $savedPath }
}
$overrides = @('HACKRF_ROOT', 'HACKRF_SOURCE_DIR', 'VCPKG_ROOT', 'VCPKG_INSTALLATION_ROOT',
              'VCPKG_OVERLAY_PORTS', 'VCPKG_OVERLAY_TRIPLETS',
              'CMAKE_PREFIX_PATH', 'CMAKE_TOOLCHAIN_FILE', 'CMAKE_GENERATOR', 'CMAKE_GENERATOR_PLATFORM',
              'CMAKE_GENERATOR_TOOLSET', 'CMAKE_GENERATOR_INSTANCE', 'CL', '_CL_', 'LINK', '_LINK_')
$savedEnvironment = @{}
foreach ($name in $overrides) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}
try {
    if (Test-Path -LiteralPath $SourceRepository) {
        $SourceRepository = [IO.Path]::GetFullPath($SourceRepository)
        $dirty = @(& git -C $SourceRepository status --porcelain --untracked-files=normal)
        if ($LASTEXITCODE -ne 0) { throw 'The source must be a Git checkout or repository URL. Clone the supplied Git bundle first.' }
        if ($dirty.Count -ne 0) { throw 'Commit the candidate before acceptance testing: cloning would otherwise test an older revision.' }
        if (-not $Revision) {
            $Revision = (& git -C $SourceRepository rev-parse HEAD)
            if ($LASTEXITCODE -ne 0) { throw 'Source checkout has no committed revision.' }
        }
    }
    Invoke-Checked 'clone' git @('clone', '--no-hardlinks', '--', $SourceRepository, $checkout)
    if ($Revision) { Invoke-Checked 'checkout' git @('-C', $checkout, 'checkout', '--detach', $Revision) }
    $script:evidence.commit = (& git -C $checkout rev-parse HEAD)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the cloned revision.' }
    Invoke-Checked 'tools' cmake @('--version')
    Push-Location $checkout
    try {
        foreach ($hackrf in @($false, $true)) {
            $preset = "windows-msvc-$($Architecture.ToLowerInvariant())"
            if ($hackrf) { $preset += '-hackrf' }
            $build = Join-Path $checkout ('build/msvc-' + $Architecture.ToLowerInvariant() + $(if ($hackrf) { '-hackrf' } else { '' }))
            $configureFailure = ''
            try {
                Invoke-Checked "$preset-configure" cmake @('--preset', $preset)
                $cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw
                if ($cache -notmatch 'CMAKE_GENERATOR:INTERNAL=Visual Studio (\d+) ' -or [int]$Matches[1] -lt 18) {
                    throw 'Acceptance requires Visual Studio 2026 or newer, discovered automatically by CMake.'
                }
                $compilerFiles = @(Get-ChildItem (Join-Path $build 'CMakeFiles') -Filter 'CMakeCCompiler.cmake' -Recurse -File)
                if ($compilerFiles.Count -ne 1 -or (Get-Content $compilerFiles[0].FullName -Raw) -notmatch 'CMAKE_C_COMPILER_ID "MSVC"') {
                    throw 'The configured C compiler is not native MSVC.'
                }
                if ($Architecture -eq 'x64') {
                    $asmFiles = @(Get-ChildItem (Join-Path $build 'CMakeFiles') -Filter 'CMakeASM_MASMCompiler.cmake' -Recurse -File)
                    if ($asmFiles.Count -ne 1 -or (Get-Content $asmFiles[0].FullName -Raw) -notmatch '(?i)ml64(?:\.exe)?') {
                        throw 'The integer FFT must be assembled with MSVC MASM/ml64.'
                    }
                }
                if ($hackrf -and $cache -match 'CMAKE_TOOLCHAIN_FILE:FILEPATH=([^\r\n]+)') {
                    $script:evidence.integratedVcpkgVerified = ($Matches[1] -replace '\\','/') -match '(?i)/VC/vcpkg/scripts/buildsystems/vcpkg\.cmake$'
                }
            } catch { $configureFailure = $_.Exception.Message }
            foreach ($configuration in @('Debug', 'Release')) {
                $label = "$preset-$($configuration.ToLowerInvariant())"
                $result = [ordered]@{ hackrf = $hackrf; configuration = $configuration; passed = $false; failure = ''; dlls = @() }
                try {
                    if ($configureFailure) { throw $configureFailure }
                    $buildPreset = $preset + $(if ($configuration -eq 'Debug') { '-debug' } else { '' })
                    Invoke-Checked "$label-build" cmake @('--build', '--preset', $buildPreset, '--parallel', '2', '--verbose')
                    $testReport = Join-Path $build "$configuration-junit.xml"
                    Invoke-Checked "$label-tests" ctest @('--preset', $buildPreset, '--no-tests=error', '--output-junit', $testReport)
                    if (-not (Test-Path -LiteralPath $testReport)) { throw 'CTest did not produce its completion report.' }
                    [xml]$testXml = Get-Content -LiteralPath $testReport -Raw
                    $suite = $testXml.DocumentElement
                    if ($suite.Name -ne 'testsuite' -or [int]$suite.GetAttribute('tests') -lt 9 -or
                        [int]$suite.GetAttribute('failures') -ne 0 -or
                        $suite.GetElementsByTagName('skipped').Count -ne 0 -or
                        $suite.GetElementsByTagName('testcase').Count -ne [int]$suite.GetAttribute('tests')) {
                        throw 'CTest report is incomplete, failed or skipped tests. Acceptance requires the whole suite.'
                    }
                    $executable = Join-Path $build "$configuration/ngene.exe"
                    Invoke-Smoke "$label-build-runtime" $executable $hackrf
                    $result.dlls = @(Get-ChildItem (Split-Path $executable -Parent) -Filter '*.dll' -File | Select-Object -ExpandProperty Name)
                    if ($hackrf -and ($result.dlls -notcontains 'hackrf.dll' -or $result.dlls -notcontains 'libusb-1.0.dll')) {
                        throw 'The clean SOURCE build must stage HackRF and libusb app-locally.'
                    }
                    $package = Join-Path $WorkRoot "$label-package"
                    Invoke-Checked "$label-install" cmake @('--install', $build, '--config', $configuration, '--prefix', (Join-Path $package 'installed'))
                    Invoke-Smoke "$label-installed-runtime" (Join-Path $package 'installed/bin/ngene.exe') $hackrf
                    Invoke-Checked "$label-package" cpack @('--config', (Join-Path $build 'CPackConfig.cmake'), '-C', $configuration, '-B', $package)
                    $archives = @(Get-ChildItem $package -Filter '*.zip' -File)
                    if ($archives.Count -ne 1) { throw 'Expected exactly one CPack ZIP.' }
                    $relocated = Join-Path $WorkRoot "$label-relocated"
                    Expand-Archive -LiteralPath $archives[0].FullName -DestinationPath $relocated
                    $executables = @(Get-ChildItem $relocated -Filter 'ngene.exe' -File -Recurse)
                    if ($executables.Count -ne 1) { throw 'Expected exactly one NGene executable in the ZIP.' }
                    Invoke-Smoke "$label-relocated-runtime" $executables[0].FullName $hackrf
                    $result.passed = $true
                } catch { $result.failure = $_.Exception.Message; Write-Warning $result.failure }
                $script:evidence.configurations += $result
            }
        }
    } finally { Pop-Location }
    $failed = @($script:evidence.configurations | Where-Object { -not $_.passed })
    if ($script:evidence.configurations.Count -ne 4 -or $failed.Count -ne 0) { throw 'Native OFF/ON x Debug/Release matrix failed. Read acceptance.json and the retained logs.' }
    if (-not $script:evidence.integratedVcpkgVerified) {
        throw 'Build matrix passed, but Visual Studio integrated vcpkg was not exercised. That acceptance criterion remains unverified.'
    }
    $script:evidence.acceptance = 'passed'
} catch {
    $script:evidence.failure = $_.Exception.Message
    throw
} finally {
    foreach ($name in $overrides) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
    $script:evidence | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $WorkRoot 'acceptance.json') -Encoding UTF8
    Write-Host "Evidence retained in $WorkRoot"
}
