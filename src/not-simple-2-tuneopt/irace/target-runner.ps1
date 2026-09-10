# target-runner.ps1 ? irace calls this for each candidate configuration
# Args from irace: <config_id> <instance_id> <seed> <instance_path> [--param val ...]

$EXE = Join-Path $PSScriptRoot "..\main.exe"
$TIME_LIMIT = 998        # 60s per evaluation ? enough signal, not too slow

$seed     = [int]$args[2]
$instance = [string]$args[3]

# Parse irace args (only alpha3 and alpha4 will be passed)
$rest = if ($args.Count -gt 4) { $args[4..($args.Count - 1)] } else { @() }
$named = @{}
$i = 0
while ($i -lt $rest.Count) {
    if ([string]$rest[$i] -match '^--') {
        $key = ([string]$rest[$i]) -replace '^--', ''
        if (($i + 1) -lt $rest.Count -and -not ([string]$rest[$i+1] -match '^--')) {
            $named[$key] = [string]$rest[$i + 1]; $i += 2
        } else { $i++ }
    } else { $i++ }
}
function Lookup-Param([string]$k, [string]$d) {
    if ($named.ContainsKey($k)) { return $named[$k] } else { return $d }
}

# === Parameters tuned by irace ===
$alpha3 = Lookup-Param 'alpha3' '1.0'
$alpha4 = Lookup-Param 'alpha4' '1.0'

# === Fixed parameters (from realbench best config on dsj1000) ===
$alpha1 = '4.04'       # pheromone importance
$alpha2 = '7.82'       # distance heuristic
$eps1   = '0.0001'     # distance smoothing (matches compare_solvers.py)
$eps2   = '0.0001'     # weight smoothing   (matches compare_solvers.py)
$rho    = '0.39'       # evaporation rate
$ants   = '500'        # number of ants
$ptries = '2'          # packing tries
$ls     = '1'          # 2-opt local search

$tmpOut = [System.IO.Path]::GetTempFileName() + ".sol"

$argList = @(
    $instance,
    '--ants',           $ants,
    '--time-limit',     $TIME_LIMIT,
    '--local-search',   $ls,
    '--alpha1',         $alpha1,
    '--alpha2',         $alpha2,
    '--alpha3',         $alpha3,
    '--alpha4',         $alpha4,
    '--eps1',           $eps1,
    '--eps2',           $eps2,
    '--rho',            $rho,
    '--delta',          '1.0',
    '--seed',           $seed,
    '--output',         $tmpOut,
    '--ptries',         $ptries,
    '--step-online',    '0',
    '--delayed-online', '0'
)

$allOut = & $EXE @argList 2>$null

$bestLine = $allOut | Where-Object { $_ -match '^Best objective:' } | Select-Object -Last 1
if ($bestLine -and $bestLine -match '([\d.eE+\-]+)\s*$') {
    $profit = [double]$Matches[1]
    Write-Output (-$profit)
} else {
    Write-Output 1e30
}

if (Test-Path $tmpOut) { Remove-Item $tmpOut -ErrorAction SilentlyContinue }
