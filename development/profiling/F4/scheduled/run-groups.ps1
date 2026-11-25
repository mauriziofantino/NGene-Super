param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Capture,
    [Parameter(Mandatory)][string]$Output,
    [ValidateRange(1, 60)][int]$Seconds = 12,
    [ValidateRange(2, 8)][int]$Rounds = 3,
    [ValidateSet('native', 'adaptive-madd')][string[]]$Profiles = @('native', 'adaptive-madd'),
    [ValidateSet('direct', 'pair', 'reuse')][string[]]$Modes = @('direct', 'pair'),
    [switch]$FixedTracking
)
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$Capture = (Resolve-Path -LiteralPath $Capture).Path
if ((Get-FileHash -LiteralPath $Capture).Hash -ne 'AA82593A27AEE92CDE07F964D9D4E41908BC129BF228A9F05A0245BAFDF22620') {
    throw 'Expected the RTL CI8 F6 capture'
}
$expected = [long]2400000 * $Seconds
if ((Get-Item -LiteralPath $Capture).Length -lt 2 * $expected) { throw 'Replay window exceeds capture' }
$null = New-Item -ItemType Directory -Force $Output
$Output = (Resolve-Path -LiteralPath $Output).Path

$cpuSource = @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class F4MachineCpu {
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetSystemTimes(out long idle, out long kernel, out long user);
    public static double BusySeconds() {
        long idle, kernel, user;
        if (!GetSystemTimes(out idle, out kernel, out user))
            throw new Win32Exception(Marshal.GetLastWin32Error());
        return (kernel + user - idle) * 1e-7;
    }
}
'@
Add-Type -TypeDefinition $cpuSource
$variables = 'NGENE_F4_PCPS_PROFILE', 'NGENE_F4_PCPS_GROUP', 'NGENE_F4_FIXED_TRACKING'
$saved = @{}
foreach ($var in $variables) { $saved[$var] = [Environment]::GetEnvironmentVariable($var) }

function Replay([int]$Round, [string]$Profile, [string]$Mode) {
    $tag = "round$Round-$Profile-$Mode"
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
cpu_budget=4
[replay]
path=$Capture
format=CI8
spectrum_inverted=false
sample_limit=$expected
realtime=true
loop=false
[dsp]
gps_fft=integer_precise
galileo_fft=integer_precise
[recording]
enabled=false
"@ | Set-Content -LiteralPath $config -Encoding ascii
    $env:NGENE_F4_PCPS_PROFILE = $Profile
    $env:NGENE_F4_PCPS_GROUP = $Mode
    $env:NGENE_F4_FIXED_TRACKING = if ($FixedTracking) { '1' } else { '0' }
    $machineStart = [F4MachineCpu]::BusySeconds()
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $Executable -ArgumentList ('--config "{0}"' -f $config) `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $ambient = [Collections.Generic.List[object]]::new()
    $previousWall = $previousProcess = 0.0
    $previousMachine = $machineStart
    $peakWorkingSet = 0L
    try {
        $null = $process.Handle
        while (!$process.WaitForExit(250)) {
            if ($timer.Elapsed.TotalSeconds -gt $Seconds + 60) {
                $process.Kill()
                throw "Timeout in $tag"
            }
            $wall = $timer.Elapsed.TotalSeconds
            $machine = [F4MachineCpu]::BusySeconds()
            $process.Refresh()
            $cpu = $process.TotalProcessorTime.TotalSeconds
            if (!$process.HasExited) { $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.PeakWorkingSet64) }
            $external = [Math]::Max(0, ($machine - $previousMachine) - ($cpu - $previousProcess))
            $ambient.Add([pscustomobject]@{elapsed_s=$wall; other_busy_cores=$external/($wall-$previousWall)})
            $previousWall = $wall
            $previousMachine = $machine
            $previousProcess = $cpu
        }
        if ($process.ExitCode -ne 0) { throw "Replay failed in $tag. See $stderr" }
        $cpu = $process.TotalProcessorTime.TotalSeconds
        $elapsed = $timer.Elapsed.TotalSeconds
        $external = [Math]::Max(0, [F4MachineCpu]::BusySeconds() - $machineStart - $cpu)
    } finally {
        $process.Dispose()
    }
    $ambient | Export-Csv -NoTypeInformation -Encoding ascii (Join-Path $run 'ambient.csv')
    $pattern = 'processed_pairs=(\d+) .*gaps=(\d+) known_input_drops=(\d+) queued_discarded_at_stop=(\d+) wall_seconds=([0-9.]+)'
    $match = [regex]::Match((Get-Content -LiteralPath $stdout -Raw), $pattern)
    if (!$match.Success -or [long]$match.Groups[1].Value -ne $expected -or
        $match.Groups[2].Value -ne '0' -or $match.Groups[3].Value -ne '0' -or
        $match.Groups[4].Value -ne '0') { throw "Incomplete replay in $tag" }
    if (!(Select-String -LiteralPath $stderr -Pattern '^f4_tracking,' -Quiet)) { throw "Missing tracking summary in $tag" }
    Write-Host "$tag pairs=$expected CPU=$cpu other_cores=$([Math]::Round($external/$elapsed, 2))"
    [pscustomobject]@{round=$Round; profile=$Profile; mode=$Mode; pairs=$expected;
        wall_s=$match.Groups[5].Value; cpu_s=$cpu; other_cores=$external/$elapsed;
        peak_working_set_bytes=$peakWorkingSet; output=$tag; fixed_tracking=[int][bool]$FixedTracking}
}
try {
    $rows = foreach ($round in 1..$Rounds) {
        $profileOrder = @($Profiles | Select-Object -Unique)
        $modeOrder = @($Modes | Select-Object -Unique)
        if ($round % 2 -eq 0) { [array]::Reverse($profileOrder); [array]::Reverse($modeOrder) }
        foreach ($profile in $profileOrder) {
            foreach ($mode in $modeOrder) { Replay $round $profile $mode }
        }
    }
    $rows | Export-Csv -NoTypeInformation -Encoding ascii (Join-Path $Output 'runs.csv')
} finally {
    foreach ($var in $variables) { [Environment]::SetEnvironmentVariable($var, $saved[$var]) }
}
