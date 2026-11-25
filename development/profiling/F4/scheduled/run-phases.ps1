param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Capture,
    [Parameter(Mandatory)][string]$Output,
    [switch]$Buffered,
    [switch]$Madd
)
$ErrorActionPreference = 'Stop'
if ($Buffered -and $Madd) { throw 'Select one candidate matrix' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$Capture = (Resolve-Path -LiteralPath $Capture).Path
if ((Get-FileHash -LiteralPath $Capture).Hash -ne '90C65DD542DE8EEE1B411FE60DF6CE24E12374B6C82261FF5FEEF213491168B1') {
    throw 'Expected the Classic F4 diagnostic capture'
}
$settings = @{
    NGENE_F4_PCPS_REPLAY = $Capture
    NGENE_F4_PCPS_OFFSET = '0'
    NGENE_F4_PCPS_FIRST_SAMPLE = '0'
    NGENE_F4_PCPS_MAP = $null
}
$previous = @{}
foreach ($key in @($settings.Keys) + @('NGENE_F4_PCPS_PROFILE', 'NGENE_F4_PCPS_PRN')) {
    $previous[$key] = [Environment]::GetEnvironmentVariable($key)
}
try {
    foreach ($key in $settings.Keys) { [Environment]::SetEnvironmentVariable($key, $settings[$key]) }
    $rows = foreach ($round in 1..4) {
        $profiles = @('native', 'adaptive', 'adaptive-avx2')
        if ($Buffered) { $profiles = @('native', 'adaptive-avx2', 'adaptive-buffered') }
        if ($Madd) { $profiles = @('native', 'adaptive-buffered', 'adaptive-madd') }
        if ($round % 2 -eq 0) { [array]::Reverse($profiles) }
        foreach ($signal in 'gps', 'galileo') {
            $prn, $grid, $expected = if ($signal -eq 'gps') { 5, 4096, 273 } else { 7, 16384, 1873 }
            $env:NGENE_F4_PCPS_PRN = "$prn"
            foreach ($profile in $profiles) {
                $env:NGENE_F4_PCPS_PROFILE = $profile
                $lines = @(& $Executable $signal 8183800 $grid 0 1 1 1)
                if ($LASTEXITCODE -or $lines.Count -ne 3) { throw "Phase trial failed for $signal/$profile" }
                $fixture = $lines[0].Split(',')
                $verify = $lines[1].Split(',')
                $phases = $lines[2].Split(',')
                if ($fixture[0] -ne 'fixture' -or $verify[0] -ne 'verify' -or $verify[10] -ne '1' -or
                    $phases[0] -ne 'fft_phases' -or [int]$phases[1] -ne $expected) {
                    throw "Unexpected amount of work for $signal/$profile"
                }
                [pscustomobject]@{round=$round; signal=$signal; prn=$prn; profile=$profile; fixture_hash=$fixture[-1];
                    map_hash=$verify[2]; fft_calls=$phases[1]; prepare_s=$phases[2];
                    stages_s=$phases[3]; restore_s=$phases[4]}
            }
        }
    }
    foreach ($group in ($rows | Group-Object signal)) {
        if (@($group.Group.fixture_hash | Select-Object -Unique).Count -ne 1 -or
            @($group.Group | Where-Object profile -ne native | Select-Object -ExpandProperty map_hash -Unique).Count -ne 1) {
            throw "Phase trials disagree on input or compact map for $($group.Name)"
        }
    }
    $rows | Export-Csv -LiteralPath $Output -NoTypeInformation -Encoding ascii
} finally {
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key]) }
}

