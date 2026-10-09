# Native Windows acceptance test. Logs and a JSON result are retained even on failure.
# The current committed revision is cloned into a new tree; no SDK or DLL preparation.
[CmdletBinding()]
param(
    [ValidateSet('x64', 'ARM64')][string]$Architecture = 'x64',
    [string]$SourceRepository = '',
    [string]$Revision = '',
    [string]$WorkRoot = (Join-Path ([IO.Path]::GetTempPath()) "ngene-msvc-$([guid]::NewGuid())")
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# Windows PowerShell 5.1 may evaluate parameter defaults before $PSScriptRoot is set.
# Resolve the checkout only after parameter binding; explicit paths/URLs stay intact.
if ([string]::IsNullOrWhiteSpace($SourceRepository)) {
    $SourceRepository = Split-Path -Path $PSScriptRoot -Parent
}
if ($env:OS -ne 'Windows_NT') { throw 'Native Windows/MSVC is required. Linux, WSL and Wine do not qualify.' }
$hostArchitecture = if ($env:PROCESSOR_ARCHITEW6432) { $env:PROCESSOR_ARCHITEW6432 } else { $env:PROCESSOR_ARCHITECTURE }
$expectedHost = if ($Architecture -eq 'x64') { 'AMD64' } else { 'ARM64' }
if ($hostArchitecture -ne $expectedHost) { throw 'Use a native target host for acceptance tests. Cross-builds use the ARM64 CMake presets.' }
$null = Get-Command git -CommandType Application -ErrorAction Stop
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
    tools = @{}; commands = @(); configurations = @(); failure = ''
}
function Get-CMakeCandidates {
    # CMake supplied by Visual Studio is commonly absent from a normal PowerShell PATH.
    # Query all installed editions, including Build Tools and preview installations.
    $onPath = @(Get-Command cmake -CommandType Application -All -ErrorAction SilentlyContinue)
    foreach ($command in $onPath) { $command.Source }
    $programRoots = @([Environment]::GetEnvironmentVariable('ProgramFiles'),
                     [Environment]::GetEnvironmentVariable('ProgramFiles(x86)'),
                     [Environment]::GetEnvironmentVariable('ProgramW6432'))
    $locators = @()
    foreach ($root in $programRoots) {
        if ($root) {
            Join-Path $root 'CMake/bin/cmake.exe'
            $locators += Join-Path $root 'Microsoft Visual Studio/Installer/vswhere.exe'
        }
    }
    $locatorCommands = @(Get-Command vswhere -CommandType Application -All -ErrorAction SilentlyContinue)
    foreach ($command in $locatorCommands) { $locators += $command.Source }
    foreach ($locator in @($locators | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $locator -PathType Leaf)) { continue }
        $savedPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $installations = @(& $locator -all -prerelease -products '*' -property installationPath 2>$null)
            if ($LASTEXITCODE -ne 0) { continue }
            foreach ($installation in $installations) {
                $path = ([string]$installation).Trim()
                if ($path) { Join-Path $path 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' }
            }
        } finally { $ErrorActionPreference = $savedPreference }
    }
}
function Get-CMakeToolVersion {
    param([string]$Executable, [string]$Name)
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $Executable --version 2>&1)
        if ($LASTEXITCODE -ne 0) { return $null }
        $match = [regex]::Match(($output -join "`n"), "(?m)^$Name version (\d+\.\d+\.\d+)")
        if ($match.Success) { return [version]$match.Groups[1].Value }
    } catch { return $null }
    finally { $ErrorActionPreference = $savedPreference }
    return $null
}
function Resolve-CMakeTools {
    param([string[]]$Candidates = @(Get-CMakeCandidates))
    $usable = @()
    $examined = @()
    foreach ($candidate in @($Candidates | Select-Object -Unique)) {
        if (-not $candidate -or -not (Test-Path -LiteralPath $candidate -PathType Leaf)) { continue }
        $directory = Split-Path -Path $candidate -Parent
        $extension = [IO.Path]::GetExtension($candidate)
        $ctest = Join-Path $directory ('ctest' + $extension)
        $cpack = Join-Path $directory ('cpack' + $extension)
        $version = Get-CMakeToolVersion $candidate 'cmake'
        $examined += "$candidate [$version]"
        if ($null -eq $version -or $version -lt [version]'4.2.0') { continue }
        if (-not (Test-Path -LiteralPath $ctest -PathType Leaf) -or
            -not (Test-Path -LiteralPath $cpack -PathType Leaf)) { continue }
        $testVersion = Get-CMakeToolVersion $ctest 'ctest'
        $packVersion = Get-CMakeToolVersion $cpack 'cpack'
        if ($version -ne $testVersion -or $version -ne $packVersion) { continue }
        $usable += [pscustomobject]@{ cmake = $candidate; ctest = $ctest; cpack = $cpack; version = $version }
    }
    if ($usable.Count -eq 0) {
        throw ('No complete CMake >=4.2 installation found in PATH, Program Files or Visual Studio. ' +
               'Install/update Visual Studio component "C++ CMake tools for Windows" or standalone CMake >=4.2. ' +
               'No PATH edit or Developer Prompt is required. Examined: ' + ($examined -join '; '))
    }
    # Never mix CMake with ctest/cpack from another installation or select an older PATH copy.
    return ($usable | Sort-Object -Property version -Descending | Select-Object -First 1)
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
function Assert-TestReport {
    param([string]$Report)
    if (-not (Test-Path -LiteralPath $Report)) { throw 'CTest did not produce its completion report.' }
    [xml]$testXml = Get-Content -LiteralPath $Report -Raw
    $suite = $testXml.DocumentElement
    # XML's "name" attribute shadows .Name in PowerShell's XML adapter; use LocalName.
    if ($suite.LocalName -ne 'testsuite' -or [int]$suite.GetAttribute('tests') -lt 9 -or
        [int]$suite.GetAttribute('failures') -ne 0 -or
        $suite.GetElementsByTagName('skipped').Count -ne 0 -or
        $suite.GetElementsByTagName('testcase').Count -ne [int]$suite.GetAttribute('tests')) {
        throw 'CTest report is incomplete, failed or skipped tests. Acceptance requires the whole suite.'
    }
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
    $tools = Resolve-CMakeTools
    $script:evidence.tools = @{ cmake = $tools.cmake; ctest = $tools.ctest; cpack = $tools.cpack; version = $tools.version.ToString() }
    Write-Host "Using CMake $($tools.version): $($tools.cmake)"
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
    Invoke-Checked 'tools' $tools.cmake @('--version')
    Push-Location $checkout
    try {
        foreach ($hackrf in @($false, $true)) {
            $preset = "windows-msvc-$($Architecture.ToLowerInvariant())"
            if ($hackrf) { $preset += '-hackrf' }
            $build = Join-Path $checkout ('build/msvc-' + $Architecture.ToLowerInvariant() + $(if ($hackrf) { '-hackrf' } else { '' }))
            $configureFailure = ''
            try {
                Invoke-Checked "$preset-configure" $tools.cmake @('--preset', $preset)
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
                    Invoke-Checked "$label-build" $tools.cmake @('--build', '--preset', $buildPreset, '--parallel', '2', '--verbose')
                    $testReport = Join-Path $build "$configuration-junit.xml"
                    Invoke-Checked "$label-tests" $tools.ctest @('--preset', $buildPreset, '--no-tests=error', '--output-junit', $testReport)
                    Assert-TestReport $testReport
                    $executable = Join-Path $build "$configuration/ngene.exe"
                    Invoke-Smoke "$label-build-runtime" $executable $hackrf
                    $result.dlls = @(Get-ChildItem (Split-Path $executable -Parent) -Filter '*.dll' -File | Select-Object -ExpandProperty Name)
                    if ($hackrf -and ($result.dlls -notcontains 'hackrf.dll' -or $result.dlls -notcontains 'libusb-1.0.dll')) {
                        throw 'The clean SOURCE build must stage HackRF and libusb app-locally.'
                    }
                    $package = Join-Path $WorkRoot "$label-package"
                    Invoke-Checked "$label-install" $tools.cmake @('--install', $build, '--config', $configuration, '--prefix', (Join-Path $package 'installed'))
                    Invoke-Smoke "$label-installed-runtime" (Join-Path $package 'installed/bin/ngene.exe') $hackrf
                    Invoke-Checked "$label-package" $tools.cpack @('--config', (Join-Path $build 'CPackConfig.cmake'), '-C', $configuration, '-B', $package)
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
