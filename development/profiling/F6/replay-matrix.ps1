param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Capture,
    [string]$Output = 'build/f6',
    [ValidateRange(1, 60)][int]$Seconds = 30,
    [ValidateRange(1, 10)][int]$Pairs = 2,
    [ValidateRange(1024, 65535)][int]$Port = 19626
)
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$Capture = (Resolve-Path -LiteralPath $Capture).Path
$null = New-Item -ItemType Directory -Force $Output
$Output = (Resolve-Path -LiteralPath $Output).Path
$rate = 2400000
$expected = [long]$rate * $Seconds
$endpoint = "http://127.0.0.1:$Port/api/v1"

function Read-Json([string]$Route) {
    Invoke-RestMethod -Uri "$endpoint/$Route" -TimeoutSec 3
}

function Measure-Replay([int]$Trial, [int]$Workers, [int]$Budget) {
    $tag = "trial$Trial-worker$Workers-budget$Budget"
    $directory = Join-Path $Output $tag
    $null = New-Item -ItemType Directory -Force $directory
    $config = Join-Path $directory 'receiver.ini'
    @"
[frontend]
type=file
sample_rate_hz=$rate
center_frequency_hz=1575420000
signal_frequency_hz=1575420000
[receiver]
run_seconds=0
channels=12
gps_reserved=8
galileo_reserved=4
[tracking]
workers=$Workers
tap_count=9
[acquisition]
cpu_budget=$Budget
[replay]
path=$Capture
realtime=true
loop=false
sample_limit=$expected
[recording]
enabled=false
"@ | Set-Content -LiteralPath $config -Encoding ascii
    $serviceArgs = '--web --port {0} --config "{1}" --output-dir "{2}"' -f $Port, $config, $directory
    $launch = @{
        FilePath = $Executable
        ArgumentList = $serviceArgs
        PassThru = $true
        WindowStyle = 'Hidden'
        RedirectStandardOutput = Join-Path $directory 'stdout.txt'
        RedirectStandardError = Join-Path $directory 'stderr.txt'
    }
    $service = Start-Process @launch
    try {
        $startup = [Diagnostics.Stopwatch]::StartNew()
        do {
            $bootstrap = $null
            try { $bootstrap = Read-Json 'bootstrap' } catch { Start-Sleep -Milliseconds 150 }
        } until ($bootstrap -or $service.HasExited -or $startup.Elapsed.TotalSeconds -ge 10)
        if (!$bootstrap -or $service.HasExited) { throw "Replay service did not start in $directory" }
        $effective = Read-Json 'config'
        if ($effective.ini -notmatch "(?m)^workers=$Workers\s*$" -or
            $effective.ini -notmatch "(?m)^cpu_budget=$Budget\s*$" -or $effective.ini -notmatch "(?m)^sample_limit=$expected\s*$") {
            throw "Unexpected effective configuration in $directory"
        }
        $request = @{
            Uri = "$endpoint/receiver/start"
            Method = 'Post'
            ContentType = 'application/json'
            Headers = @{'X-NGene-Token' = $bootstrap.token}
            Body = @{revision = $effective.revision} | ConvertTo-Json -Compress
            TimeoutSec = 3
        }
        $service.Refresh()
        $startCpu = $service.TotalProcessorTime.TotalSeconds
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $null = Invoke-RestMethod @request
        $points = [Collections.Generic.List[object]]::new()
        $running = $false
        do {
            $status = Read-Json 'status'
            $service.Refresh()
            if ($status.state -eq 'running') { $running = $true }
            $points.Add([ordered]@{
                elapsed = $timer.Elapsed.TotalSeconds
                cpu = $service.TotalProcessorTime.TotalSeconds - $startCpu
                state = $status.state
                sample = [long]$status.processed_samples
                ring = [long]$status.ring_high_water
                ingress = [long]$status.ingress_high_water_blocks
                ingress_available = $status.ingress_available
                gaps = [long]$status.gap_events
                drops = [long]$status.known_dropped_samples
                discarded = [long]$status.input_discarded_on_stop
                gps = $status.acquisition
                galileo = $status.galileo_acquisition
                tracking = @($status.tracking | Where-Object active | Select-Object prn,constellation,pll,tow_valid,first_sample,end_sample,epoch,integrations)
                pvt = $status.pvt | Select-Object available,epoch,sample,gps,galileo
            })
            if ($status.state -eq 'error') { throw "Receiver error: $($status.message)" }
            if ($running -and $status.state -notin @('running', 'stopping')) { break }
            Start-Sleep -Milliseconds 250
        } while ($timer.Elapsed.TotalSeconds -lt $Seconds + 20)
        $points | ConvertTo-Json -Depth 12 -Compress | Set-Content -LiteralPath (Join-Path $directory 'timeline.json') -Encoding ascii
        $last = $points[-1]
        if (!$running -or $last.state -eq 'running' -or $last.sample -ne $expected) {
            throw "Incomplete run $tag, processed $($last.sample) of $expected"
        }
        Write-Host "$tag processed=$($last.sample) gaps=$($last.gaps) cpu_seconds=$($last.cpu)"
    } finally {
        if (!$service.HasExited) { Stop-Process -InputObject $service }
        $service.Dispose()
    }
}

$conditions = @(@(1, 1), @(4, 1), @(4, 4), @(1, 4))
for ($trial = 1; $trial -le $Pairs; ++$trial) {
    $order = @(0, 1, 2, 3)
    if ($trial % 2 -eq 0) { [array]::Reverse($order) }
    foreach ($index in $order) {
        $workers, $budget = $conditions[$index]
        Measure-Replay $trial $workers $budget
    }
}
