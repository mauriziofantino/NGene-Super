param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Capture,
    [Parameter(Mandatory)][string]$Output,
    [ValidateRange(1, 60)][int]$Seconds = 12,
    [ValidateRange(1, 6)][int]$Rounds = 2,
    [switch]$AdaptiveSimd
)
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$Capture = (Resolve-Path -LiteralPath $Capture).Path
$metadata = Get-Content -LiteralPath "$Capture.meta.ini" -Raw
if ($metadata -notmatch '(?m)^format=CI8\s*$' -or
    $metadata -notmatch '(?m)^sample_rate_hz=2400000\s*$' -or
    $metadata -notmatch '(?m)^digital_if_hz=0\s*$' -or
    $metadata -notmatch '(?m)^center_frequency_hz=1575420000\s*$' -or
    $metadata -notmatch '(?m)^signal_frequency_hz=1575420000\s*$' -or
    $metadata -notmatch '(?m)^spectrum_inverted=0\s*$') {
    throw 'Expected a CI8 capture with a 2.4 Msps sidecar'
}
$expected = [long]2400000 * $Seconds
if ((Get-Item -LiteralPath $Capture).Length -lt 2 * $expected) {
    throw 'Capture is shorter than the requested replay'
}
$null = New-Item -ItemType Directory -Force $Output
$Output = (Resolve-Path -LiteralPath $Output).Path
$profileVariable = 'NGENE_F4_PCPS_PROFILE'
$previousProfile = [Environment]::GetEnvironmentVariable($profileVariable)
$rows = foreach ($round in 1..$Rounds) {
    $budgets = @(1, 4)
    $profiles = @('native', 'wide', 'adaptive')
    if ($AdaptiveSimd) {
        $budgets = @(4)
        $profiles = @('native', 'adaptive', 'adaptive-avx2')
    }
    if ($round % 2 -eq 0) {
        [array]::Reverse($budgets)
        [array]::Reverse($profiles)
    }
    foreach ($budget in $budgets) {
        foreach ($profile in $profiles) {
            $tag = "round$round-budget$budget-$profile"
            $run = Join-Path $Output $tag
            $null = New-Item -ItemType Directory -Force $run
            $config = Join-Path $run 'receiver.ini'
            $stdout = Join-Path $run 'stdout.txt'
            $stderr = Join-Path $run 'stderr.txt'
            @"
[frontend]
type=file
sample_rate_hz=2400000
center_frequency_hz=1575420000
signal_frequency_hz=1575420000
[receiver]
run_seconds=0
channels=12
gps_reserved=8
galileo_reserved=4
[buffer]
window_samples=2400
[tracking]
workers=4
tap_count=9
[acquisition]
cpu_budget=$budget
[replay]
path=$Capture
format=CI8
spectrum_inverted=false
start_sample=0
sample_limit=$expected
realtime=true
loop=false
[dsp]
gps_fft=integer_precise
galileo_fft=integer_precise
[recording]
enabled=false
"@ | Set-Content -LiteralPath $config -Encoding ascii
            [Environment]::SetEnvironmentVariable($profileVariable, $profile)
            $process = $null
            try {
                $process = Start-Process -FilePath $Executable -ArgumentList ('--config "{0}"' -f $config) `
                    -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
                $null = $process.Handle
                if (!$process.WaitForExit(1000 * ($Seconds + 60))) {
                    $process.Kill()
                    throw "Replay timed out for $tag"
                }
                if ($process.ExitCode -ne 0) { throw "Replay failed for $tag. See $stderr" }
                $cpu = $process.TotalProcessorTime.TotalSeconds
            } finally {
                if ($process) { $process.Dispose() }
                [Environment]::SetEnvironmentVariable($profileVariable, $previousProfile)
            }
            $stats = Get-Content -LiteralPath $stdout -Raw
            $pattern = 'processed_pairs=(\d+) .*gaps=(\d+) known_input_drops=(\d+) queued_discarded_at_stop=(\d+) wall_seconds=([0-9.]+)'
            $match = [regex]::Match($stats, $pattern)
            if (!$match.Success -or [long]$match.Groups[1].Value -ne $expected -or
                $match.Groups[2].Value -ne '0' -or $match.Groups[3].Value -ne '0' -or
                $match.Groups[4].Value -ne '0') {
                throw "Incomplete or discontinuous replay for $tag. See $stdout"
            }
            if (!(Select-String -LiteralPath $stderr -Pattern '^f6_job,' -Quiet)) {
                throw "Missing job trace for $tag"
            }
            Write-Host "$tag processed=$expected cpu=$cpu"
            [pscustomobject]@{round=$round; budget=$budget; profile=$profile; pairs=$expected;
                wall_seconds=$match.Groups[5].Value; cpu_seconds=$cpu}
        }
    }
}
$rows | Export-Csv -LiteralPath (Join-Path $Output 'runs.csv') -NoTypeInformation -Encoding ascii

