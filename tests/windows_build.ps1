# Host contracts for the PowerShell/VS Code entry point, not a native MSVC run.
param([Parameter(Mandatory=$true)][string]$Source,
      [Parameter(Mandatory=$true)][string]$Work)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$powerShell = (Get-Process -Id $PID).Path
$entry = Join-Path $Source 'scripts/build-windows.ps1'
$tokens = $null; $errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($entry, [ref]$tokens, [ref]$errors)
if ($errors.Count -ne 0) { throw ($errors | Out-String) }
$presets = Get-Content -LiteralPath (Join-Path $Source 'CMakePresets.json') -Raw | ConvertFrom-Json
foreach ($architecture in @('x64', 'ARM64')) {
    foreach ($hackrf in @('OFF', 'ON')) {
        foreach ($configuration in @('Debug', 'Release')) {
            $json = & $powerShell -NoLogo -NoProfile -File $entry -Architecture $architecture -Hackrf $hackrf `
                -Configuration $configuration -Action Test -ShowPlan
            if ($LASTEXITCODE -ne 0) { throw 'Build planning failed.' }
            $plan = ($json -join "`n") | ConvertFrom-Json
            if ($presets.configurePresets.name -notcontains $plan.configurePreset -or
                $presets.buildPresets.name -notcontains $plan.buildPreset -or
                $presets.testPresets.name -notcontains $plan.buildPreset -or
                $plan.commands.Count -ne 3 -or $plan.commands[2].tool -ne 'ctest') {
                throw 'Developer build/test plan does not use the committed presets.'
            }
        }
    }
}
$tasks = Get-Content -LiteralPath (Join-Path $Source '.vscode/tasks.json') -Raw | ConvertFrom-Json
$buildTask = @($tasks.tasks | Where-Object { $_.label -eq 'NGene: Build (Windows MSVC)' })
if ($buildTask.Count -ne 1 -or $buildTask[0].type -ne 'process' -or
    $buildTask[0].args -notcontains '${workspaceFolder}/scripts/build-windows.ps1' -or
    -not $buildTask[0].group.isDefault) { throw 'VS Code default build does not use the PowerShell entry point.' }
# Execute the actual entry point using fixture tools. This validates orchestration
# and exit propagation on any host; no Windows binary or compiler is simulated.
$fixture = Join-Path $Work 'developer entry with spaces'
$scripts = Join-Path $fixture 'scripts'
$null = New-Item -Path $scripts -ItemType Directory -Force
Copy-Item -LiteralPath $entry -Destination (Join-Path $scripts 'build-windows.ps1')
Copy-Item -LiteralPath (Join-Path $Source 'scripts/windows-tools.ps1') -Destination (Join-Path $scripts 'windows-tools.ps1')
$mock = @'
function Resolve-CMakeTools {
    return [pscustomobject]@{
        cmake = (Join-Path $PSScriptRoot 'cmake.ps1')
        ctest = (Join-Path $PSScriptRoot 'ctest.ps1')
        cpack = (Join-Path $PSScriptRoot 'cpack.ps1')
        version = [version]'4.3.0'
    }
}
'@
Add-Content -LiteralPath (Join-Path $scripts 'windows-tools.ps1') -Value $mock -Encoding UTF8
$fakeCmake = @'
if ($args[0] -eq '--preset') {
    $build = Join-Path (Split-Path $PSScriptRoot -Parent) 'build/msvc-x64-hackrf'
    $null = New-Item -Path $build -ItemType Directory -Force
    Set-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Value 'CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026'
    foreach ($target in @('ngene', 'ngene_core', 'ngene_control', 'ngene_http')) {
        $level = if ($target -eq 'ngene_http') { 'TurnOffAllWarnings' } else { 'Level4' }
        $xml = "<Project><ItemDefinitionGroup><ClCompile><WarningLevel>$level</WarningLevel><TreatWarningAsError>true</TreatWarningAsError></ClCompile></ItemDefinitionGroup></Project>"
        Set-Content -LiteralPath (Join-Path $build "$target.vcxproj") -Value $xml
    }
}
Write-Output 'Fixture configure/build completed'
exit 0
'@
Set-Content -LiteralPath (Join-Path $scripts 'cmake.ps1') -Value $fakeCmake -Encoding UTF8
Set-Content -LiteralPath (Join-Path $scripts 'ctest.ps1') -Value "Write-Output 'Fixture tests completed'; exit 0" -Encoding UTF8
$environmentNames = @('OS', 'PROCESSOR_ARCHITECTURE', 'PROCESSOR_ARCHITEW6432')
$savedEnvironment = @{}
foreach ($name in $environmentNames) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $env:OS = 'Windows_NT'; $env:PROCESSOR_ARCHITECTURE = 'AMD64'
    [Environment]::SetEnvironmentVariable('PROCESSOR_ARCHITEW6432', $null, 'Process')
    $fixtureEntry = Join-Path $scripts 'build-windows.ps1'
    & $powerShell -NoLogo -NoProfile -File $fixtureEntry -Action Test
    if ($LASTEXITCODE -ne 0) { throw 'Developer orchestration fixture failed.' }
    Set-Content -LiteralPath (Join-Path $scripts 'ctest.ps1') -Value "Write-Output 'Fixture test failure'; exit 8" -Encoding UTF8
    $savedPreference = $ErrorActionPreference
    try {
        # Windows PowerShell 5.1 represents child stderr as ErrorRecords. Retain
        # it as data so the assertion checks the child's actual nonzero exit.
        $ErrorActionPreference = 'Continue'
        $failure = @(& $powerShell -NoLogo -NoProfile -File $fixtureEntry -Action Test 2>&1)
        $failureExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
    if ($failureExit -eq 0 -or ($failure -join "`n") -notmatch 'test failed \(exit 8\)') {
        throw 'CTest failure was not propagated to the VS Code task/PowerShell caller.'
    }
} finally {
    foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
}
Write-Host 'PASS: preset plans, VS Code task, developer orchestration and failing CTest exit (host fixture only)'
