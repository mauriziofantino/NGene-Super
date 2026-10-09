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
Write-Host 'PASS: verifier entry point, syntax, exits, compiler/localization/policy diagnostics and complete JUnit report guard (helper contract only)'
