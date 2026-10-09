# Portable contract for the verification helper. Does not execute or certify MSVC.
param([Parameter(Mandatory=$true)][string]$Source,
      [Parameter(Mandatory=$true)][string]$Work)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $Source 'scripts/verify-windows.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count -ne 0) { throw ($errors | Out-String) }
# Do not regress the WinPS 5.1 entry-point failure: automatic script-path variables
# must be used in the script body, never in a parameter's default expression.
foreach ($parameter in $ast.ParamBlock.Parameters) {
    if ($null -ne $parameter.DefaultValue) {
        $earlyPath = $parameter.DefaultValue.Find({
            param($node)
            $node -is [System.Management.Automation.Language.VariableExpressionAst] -and
                $node.VariablePath.UserPath -in @('PSScriptRoot', 'PSCommandPath', 'MyInvocation')
        }, $true)
        if ($null -ne $earlyPath) { throw 'Script-path resolution occurs before parameter binding.' }
    }
}
$entryDirectory = Join-Path $Work 'entry point with spaces/scripts'
$null = New-Item -Path $entryDirectory -ItemType Directory -Force
$entryFixture = Join-Path $entryDirectory 'verify entry.ps1'
$platformGuard = @($ast.EndBlock.Statements | Where-Object {
    $_.Extent.Text.StartsWith('if ($env:OS')
})
if ($platformGuard.Count -ne 1) { throw 'Cannot isolate the verifier entry point.' }
$sourceText = Get-Content -LiteralPath (Join-Path $Source 'scripts/verify-windows.ps1') -Raw
$entryText = $sourceText.Substring(0, $platformGuard[0].Extent.StartOffset) +
    [Environment]::NewLine + 'Write-Output $SourceRepository'
Set-Content -LiteralPath $entryFixture -Value $entryText -Encoding UTF8
$expectedSource = Split-Path -Path $entryDirectory -Parent
Push-Location $Source
try {
    $resolvedSource = & $entryFixture
    if ($resolvedSource -ne $expectedSource) { throw 'Default checkout depends on the current directory.' }
    foreach ($explicitSource in @($Source, 'https://example.invalid/ngene.git')) {
        $resolvedSource = & $entryFixture -SourceRepository $explicitSource
        if ($resolvedSource -ne $explicitSource) { throw 'Explicit repository path or URL was changed.' }
    }
} finally { Pop-Location }
. (Join-Path $Source 'scripts/windows-tools.ps1')
$toolRoot = Join-Path $Work 'tool discovery with spaces'
function New-ToolFixture {
    param([string]$Directory, [string]$Version, [string]$PackVersion = $Version)
    $null = New-Item -Path $Directory -ItemType Directory -Force
    foreach ($name in @('cmake', 'ctest', 'cpack')) {
        $toolVersion = if ($name -eq 'cpack') { $PackVersion } else { $Version }
        $content = "Write-Output '$name version $toolVersion'; exit 0"
        Set-Content -LiteralPath (Join-Path $Directory "$name.ps1") -Value $content -Encoding UTF8
    }
    return (Join-Path $Directory 'cmake.ps1')
}
$oldTool = New-ToolFixture (Join-Path $toolRoot 'old PATH copy') '3.31.0'
$goodTool = New-ToolFixture (Join-Path $toolRoot 'Visual Studio') '4.2.0'
$newTool = New-ToolFixture (Join-Path $toolRoot 'standalone') '4.4.0'
$mixedTool = New-ToolFixture (Join-Path $toolRoot 'mismatched') '4.5.0' '4.4.0'
$missingTool = New-ToolFixture (Join-Path $toolRoot 'incomplete') '4.6.0'
Remove-Item -LiteralPath (Join-Path (Split-Path $missingTool -Parent) 'ctest.ps1')
$selected = Resolve-CMakeTools -Candidates @($oldTool, $mixedTool, $missingTool, $goodTool, $newTool)
if ($selected.cmake -ne $newTool -or $selected.ctest -ne (Join-Path (Split-Path $newTool -Parent) 'ctest.ps1')) {
    throw 'Failed to select one complete compatible suite from the discovered tools.'
}
$selected = Resolve-CMakeTools -Candidates @($oldTool, $goodTool)
if ($selected.cmake -ne $goodTool) { throw 'An old PATH copy hides compatible Visual Studio CMake.' }
$caught = $false
try { Resolve-CMakeTools -Candidates @($oldTool, $mixedTool, $missingTool) | Out-Null }
catch {
    $caught = $_.Exception.Message.Contains('No complete CMake >=4.2 installation found')
}
if (-not $caught) { throw 'Missing/incompatible tools were not rejected with an actionable error.' }
# Query a real fixture locator with no cmake command available: exercise discovery
# separately from tool selection, without depending on the test host's installations.
$locatorFixture = Join-Path $toolRoot 'vswhere fixture.ps1'
$installationFixture = Join-Path $toolRoot 'VS arbitrary edition'
Set-Content -LiteralPath $locatorFixture -Encoding UTF8 -Value (
    "Write-Output '$($installationFixture.Replace("'", "''"))'; exit 0")
function Get-Command {
    param([string]$Name, [string]$CommandType, [switch]$All, [string]$ErrorAction)
    if ($Name -eq 'vswhere') { return [pscustomobject]@{ Source = $locatorFixture } }
}
try {
    $candidates = @(Get-CMakeCandidates)
    $expectedTool = Join-Path $installationFixture 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    if ($candidates -notcontains $expectedTool) { throw 'Visual Studio CMake was not found when absent from PATH.' }
} finally { Remove-Item Function:Get-Command }
if ($null -eq (Get-Command Assert-VsWarningPolicy -CommandType Function)) {
    throw 'Missing generated MSBuild warning policy guard.'
}
$projectFixture = Join-Path $toolRoot 'warnings.vcxproj'
$vendorProject = '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">' +
    '<ItemDefinitionGroup><ClCompile><WarningLevel>TurnOffAllWarnings</WarningLevel>' +
    '<AdditionalOptions>%(AdditionalOptions)</AdditionalOptions></ClCompile></ItemDefinitionGroup></Project>'
$ownProject = $vendorProject.Replace('TurnOffAllWarnings', 'Level4').Replace(
    '</WarningLevel>', '</WarningLevel><TreatWarningAsError>true</TreatWarningAsError>')
Set-Content -LiteralPath $projectFixture -Value $vendorProject -Encoding UTF8
Assert-VsWarningPolicy $projectFixture $true
Set-Content -LiteralPath $projectFixture -Value $ownProject -Encoding UTF8
Assert-VsWarningPolicy $projectFixture $false
foreach ($case in @(
    @{ xml = $vendorProject.Replace('TurnOffAllWarnings','Level1'); external = $true },
    @{ xml = $vendorProject.Replace('%(AdditionalOptions)', '/w %(AdditionalOptions)'); external = $true },
    @{ xml = $ownProject.Replace('Level4','Level1'); external = $false },
    @{ xml = $ownProject.Replace('>true<','>false<'); external = $false }
)) {
    Set-Content -LiteralPath $projectFixture -Value $case.xml -Encoding UTF8
    $caught = $false
    try { Assert-VsWarningPolicy $projectFixture $case.external } catch { $caught = $true }
    if (-not $caught) { throw 'Conflicting/relaxed generated MSVC warning policy was accepted.' }
}
$helper = $ast.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-Checked'
}, $true)
if ($null -eq $helper) { throw 'Missing native command verification helper.' }
Invoke-Expression $helper.Extent.Text
$reportHelper = $ast.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Assert-TestReport'
}, $true)
if ($null -eq $reportHelper) { throw 'Missing CTest completion verification helper.' }
Invoke-Expression $reportHelper.Extent.Text
$logs = Join-Path $Work 'verifier-helper'
$null = New-Item -Path $logs -ItemType Directory -Force
$script:commandNumber = 0
$script:evidence = @{ commands = @() }
$powerShell = (Get-Process -Id $PID).Path
Invoke-Checked 'success' $powerShell @('-NoLogo','-NoProfile','-Command','[Console]::WriteLine(''OK'')')
$cases = @(
    @{ label = 'warning'; command = '[Console]::WriteLine(''source.c: warning C4701: test warning'')' },
    @{ label = 'localized'; command = '[Console]::WriteLine(''source.c: avviso C4244: test warning'')' },
    @{ label = 'failure'; command = '[Console]::Error.WriteLine(''failure''); exit 9' },
    @{ label = 'policy'; command = '[Console]::Error.WriteLine(''CMake Warning (dev): test policy'')' }
)
foreach ($case in $cases) {
    $caught = $false
    try { Invoke-Checked $case.label $powerShell @('-NoLogo','-NoProfile','-Command',$case.command) }
    catch { $caught = $true }
    if (-not $caught) { throw "Missed $($case.label) failure." }
}
if ($script:evidence.commands.Count -ne 5 -or
    $script:evidence.commands[1].warnings -ne 1 -or
    $script:evidence.commands[2].warnings -ne 1 -or
    $script:evidence.commands[3].exitCode -ne 9) { throw 'Lost or incorrect command evidence.' }
$report = Join-Path $logs 'junit.xml'
$casesXml = '<testcase name="fixture"/>' * 9
$valid = '<testsuite name="(empty)" tests="9" failures="0">' + $casesXml + '</testsuite>'
Set-Content -LiteralPath $report -Value $valid -Encoding UTF8
Assert-TestReport $report
foreach ($invalid in @(
    $valid.Replace('failures="0"', 'failures="1"'),
    $valid.Replace('tests="9"', 'tests="10"'),
    $valid.Replace('<testcase name="fixture"/>', '<testcase><skipped/></testcase>'),
    '<testsuite name="(empty)" tests="0" failures="0"/>',
    '<testsuites/>'
)) {
    Set-Content -LiteralPath $report -Value $invalid -Encoding UTF8
    $caught = $false
    try { Assert-TestReport $report } catch { $caught = $true }
    if (-not $caught) { throw 'Incomplete, failed or skipped CTest report was accepted.' }
}
Write-Host 'PASS: verifier entry point, CMake discovery/selection, MSBuild warning policy, syntax, exits, compiler/localization/policy diagnostics and complete JUnit report guard (helper contract only)'
