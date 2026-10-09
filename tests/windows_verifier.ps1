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
$helper = $ast.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-Checked'
}, $true)
if ($null -eq $helper) { throw 'Missing native command verification helper.' }
Invoke-Expression $helper.Extent.Text
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
Write-Host 'PASS: verifier syntax, nonzero exits, compiler diagnostics/localization and policy warnings (helper contract only)'
