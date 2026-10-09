# Shared tool discovery for native Windows build and acceptance entry points.
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
function Assert-VsWarningPolicy {
    param([string]$ProjectFile, [bool]$External)
    if (-not (Test-Path -LiteralPath $ProjectFile -PathType Leaf)) { throw "Missing MSBuild project: $ProjectFile" }
    [xml]$projectXml = Get-Content -LiteralPath $ProjectFile -Raw
    $settings = @($projectXml.SelectNodes("//*[local-name()='ItemDefinitionGroup']/*[local-name()='ClCompile']"))
    if ($settings.Count -eq 0) { throw "No compiler settings in MSBuild project: $ProjectFile" }
    $expectedLevel = if ($External) { 'TurnOffAllWarnings' } else { 'Level4' }
    foreach ($setting in $settings) {
        $level = $setting.SelectSingleNode("*[local-name()='WarningLevel']")
        $extra = $setting.SelectSingleNode("*[local-name()='AdditionalOptions']")
        if ($null -eq $level -or $level.InnerText -ne $expectedLevel -or
            ($null -ne $extra -and $extra.InnerText -cmatch '(?<!\S)/w(?!\S)')) {
            throw "Conflicting or incorrect MSVC warning level in $ProjectFile (expected $expectedLevel)."
        }
        if (-not $External) {
            $asError = $setting.SelectSingleNode("*[local-name()='TreatWarningAsError']")
            if ($null -eq $asError -or $asError.InnerText -ne 'true') {
                throw "NGene must retain /WX in $ProjectFile."
            }
        }
    }
}
